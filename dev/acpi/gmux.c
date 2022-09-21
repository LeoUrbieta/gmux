/*
 * Driver to control backlight in MacBook Retina Display.
 * Address numbers and inner workings heavily derived from
 * apple-gmux Linux driver.
 *
 * */


#include <sys/param.h>
#include <sys/systm.h>

#include <dev/acpi/amltypes.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/dsdt.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>

#ifdef GMUX_DEBUG
#define DPRINTF(x) printf x
#else
#define DPRINTF(x)
#endif

#define GMUX_PORT_BRIGHTNESS		0x74
#define GMUX_PORT_VALUE			0xc2
#define GMUX_PORT_READ			0xd0
#define GMUX_PORT_WRITE			0xd4

#define GMUX_MIN_BRIGHTNESS		0
#define GMUX_MAX_BRIGHTNESS		1023

struct gmux_softc {
	struct device		 sc_dev;

	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_devnode;

	bus_space_tag_t		 sc_iot;
	bus_space_handle_t	 sc_ioh;

	uint16_t		 sc_brightness;
};

int	gmux_match(struct device *, void *, void *);
void	gmux_attach(struct device *, struct device *, void *);
void	gmux_complete(struct gmux_softc *);
void	gmux_ready(struct gmux_softc *);
bool	gmux_confirm_retina_display(struct gmux_softc *);
int	gmux_get_brightness(struct gmux_softc *);
int	gmux_set_brightness(struct gmux_softc *,uint16_t);

/* Hooks for wsconsole brightness control. */
int	gmux_get_param(struct wsdisplay_param *);
int	gmux_set_param(struct wsdisplay_param *);

const struct cfattach gmux_ca = {
	sizeof(struct gmux_softc), gmux_match, gmux_attach, NULL, NULL
};

struct cfdriver gmux_cd = {
	NULL, "gmux", DV_DULL
};

const char *gmux_hids[] = {
	"APP000B", NULL
};

int
gmux_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args *aa = aux;
	struct cfdata *cf = match;
	
	return acpi_matchhids(aa, gmux_hids, cf->cf_driver->cd_name);
	
}

void
gmux_attach(struct device *parent, struct device *self, void *aux)
{
	struct gmux_softc *sc = (struct gmux_softc *)self;
	struct acpi_attach_args *aaa = aux;
	struct aml_value res;
	int64_t sta;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_devnode = aaa->aaa_node;

	printf(": %s", sc->sc_devnode->name);

        sta = acpi_getsta(sc->sc_acpi, sc->sc_devnode);
	if ((sta & (STA_PRESENT | STA_ENABLED | STA_DEV_OK)) !=
	    (STA_PRESENT | STA_ENABLED | STA_DEV_OK)) {
		printf(": not enabled\n");
		return;
	}

	if (!(aml_evalname(sc->sc_acpi, sc->sc_devnode, "_CID", 0, NULL, &res)))
		printf(" (%s)", res.v_string);

	if (strncmp(hw_prod, "MacBookPro11,5", 14)){
		printf("\n");
		return;
	}

	sc->sc_iot = aaa->aaa_iot;
	if (bus_space_map(sc->sc_iot, aaa->aaa_addr[0], aaa->aaa_size[0], 0,
	    &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	if (!gmux_confirm_retina_display(sc))
		return;

	/* Set initial brightness to maximum */
	sc->sc_brightness = gmux_set_brightness(sc,GMUX_MAX_BRIGHTNESS);

	/* Map wsconsole hook functions. */
	ws_get_param = gmux_get_param;
	ws_set_param = gmux_set_param;

	printf("\n");
}

bool 
gmux_confirm_retina_display(struct gmux_softc *sc)
{
	uint16_t retina_value;

	/* Values that confirm that gmux chip is for retina display*/
	bus_space_write_1(sc->sc_iot,sc->sc_ioh,0xcc,0xaa);
	bus_space_write_1(sc->sc_iot,sc->sc_ioh,0xcd,0x55);
	bus_space_write_1(sc->sc_iot,sc->sc_ioh,0xce,0x00);

	retina_value = bus_space_read_2(sc->sc_iot,sc->sc_ioh,0xcc) | 
		(bus_space_read_1(sc->sc_iot,sc->sc_ioh,0xcd) << 8); 
	if (retina_value == 0x55aa)
		return true;
	else
		return false;
}

void 
gmux_complete(struct gmux_softc *sc)
{
	int counter = 50;
	uint8_t complete;

	complete = bus_space_read_1(sc->sc_iot,sc->sc_ioh,GMUX_PORT_WRITE);
	while (counter && !(complete & 0x01)){
		complete = bus_space_read_1(sc->sc_iot,sc->sc_ioh,
				GMUX_PORT_WRITE);
		delay(10);
		counter--;
	}		

	if (complete & 0x01)
		bus_space_read_1(sc->sc_iot,sc->sc_ioh,GMUX_PORT_WRITE);

}

void 
gmux_ready(struct gmux_softc *sc)
{
	int counter = 50;
	uint8_t ready;
	ready = bus_space_read_1(sc->sc_iot,sc->sc_ioh,GMUX_PORT_WRITE);
	while (counter && (ready & 0x01)){
		bus_space_read_1(sc->sc_iot,sc->sc_ioh,GMUX_PORT_READ);
		ready = bus_space_read_1(sc->sc_iot,sc->sc_ioh,
				GMUX_PORT_WRITE);
		delay(10);
		counter--;
	}
}

int 
gmux_get_brightness(struct gmux_softc *sc)
{
	gmux_ready(sc);
	bus_space_write_1(sc->sc_iot,sc->sc_ioh,GMUX_PORT_READ,
			GMUX_PORT_BRIGHTNESS);
	gmux_complete(sc);
	return bus_space_read_4(sc->sc_iot,sc->sc_ioh,GMUX_PORT_VALUE);	
}

int 
gmux_set_brightness(struct gmux_softc *sc, uint16_t brightness)
{
	bus_space_write_4(sc->sc_iot,sc->sc_ioh,GMUX_PORT_VALUE,brightness);
	
	gmux_ready(sc);
	bus_space_write_1(sc->sc_iot,sc->sc_ioh,GMUX_PORT_WRITE,
			GMUX_PORT_BRIGHTNESS);
	gmux_complete(sc);
	return brightness;
}

int 
gmux_get_param(struct wsdisplay_param *dp)
{
	struct gmux_softc *sc = gmux_cd.cd_devs[0];

	if (sc == NULL)
		return -1;
	
	switch (dp->param) {
	case WSDISPLAYIO_PARAM_BACKLIGHT:
		DPRINTF(("gmux_get_param: sc->sc_brightness = %d\n",
		    sc->sc_brightness));
		dp->min = GMUX_MIN_BRIGHTNESS;
		dp->max = GMUX_MAX_BRIGHTNESS;
		dp->curval = sc->sc_brightness;
		return 0;
	default:
		return -1;
	}
}


int gmux_set_param(struct wsdisplay_param *dp)
{
	struct gmux_softc *sc = gmux_cd.cd_devs[0];

	if (sc == NULL)
		return -1;

	switch (dp->param) {
	case WSDISPLAYIO_PARAM_BACKLIGHT:
		DPRINTF(("gmux_set_param: curval = %d\n", dp->curval));
		if (dp->curval < GMUX_MIN_BRIGHTNESS)
			dp->curval = 0;
		if (dp->curval > GMUX_MAX_BRIGHTNESS)
			dp->curval = GMUX_MAX_BRIGHTNESS;
		gmux_set_brightness(sc,dp->curval);
		sc->sc_brightness = dp->curval;
		return 0;
	default:
		return -1;
	}
}
