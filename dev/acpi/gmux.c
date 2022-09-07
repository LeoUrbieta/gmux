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

#define GMUX_PORT_VERSION_MAJOR		0x04
#define GMUX_PORT_VERSION_MINOR		0x05
#define GMUX_PORT_VERSION_RELEASE	0x06
#define GMUX_PORT_SWITCH_DISPLAY	0x10
#define GMUX_PORT_SWITCH_GET_DISPLAY	0x11
#define GMUX_PORT_INTERRUPT_ENABLE	0x14
#define GMUX_PORT_INTERRUPT_STATUS	0x16
#define GMUX_PORT_SWITCH_DDC		0x28
#define GMUX_PORT_SWITCH_EXTERNAL	0x40
#define GMUX_PORT_SWITCH_GET_EXTERNAL	0x41
#define GMUX_PORT_DISCRETE_POWER	0x50
#define GMUX_PORT_MAX_BRIGHTNESS	0x70
#define GMUX_PORT_BRIGHTNESS		0x74
#define GMUX_PORT_VALUE			0xc2
#define GMUX_PORT_READ			0xd0
#define GMUX_PORT_WRITE			0xd4

#define GMUX_MIN_IO_LEN			(GMUX_PORT_BRIGHTNESS + 4)

#define GMUX_INTERRUPT_ENABLE		0xff
#define GMUX_INTERRUPT_DISABLE		0x00

#define GMUX_INTERRUPT_STATUS_ACTIVE	0
#define GMUX_INTERRUPT_STATUS_DISPLAY	(1 << 0)
#define GMUX_INTERRUPT_STATUS_POWER	(1 << 2)
#define GMUX_INTERRUPT_STATUS_HOTPLUG	(1 << 3)

#define GMUX_BRIGHTNESS_MASK		0x00ffffff
#define GMUX_MAX_BRIGHTNESS		GMUX_BRIGHTNESS_MASK

#define GMUX_MIN_BRIGHTNESS		0
#define GMUX_MAPPED_MAX_BRIGHTNESS	1000
#define GMUX_REAL_MAX_BRIGHTNESS	67113984

struct gmux_softc {
	struct device		 sc_dev;

	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_devnode;

	bus_space_tag_t		 sc_iot;
	bus_space_handle_t	 sc_ioh;

	uint32_t		 sc_brightness;
};

int	gmux_match(struct device *, void *, void *);
void	gmux_attach(struct device *, struct device *, void *);
void    gmux_complete(struct gmux_softc *);
void 	gmux_version(struct gmux_softc *, int);
int	gmux_get_brightness(struct gmux_softc *);
void    gmux_set_brightness(struct gmux_softc *,uint32_t);
int 	map_value(uint32_t);
int     inverse_map_value(uint32_t);

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
	uint8_t data;
	uint16_t val;

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

	sc->sc_iot = aaa->aaa_iot;
	if (bus_space_map(sc->sc_iot, aaa->aaa_addr[0], aaa->aaa_size[0], 0,
	    &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	bus_space_write_1(sc->sc_iot,sc->sc_ioh,0xcc,0xaa);
	bus_space_write_1(sc->sc_iot,sc->sc_ioh,0xcd,0x55);
	bus_space_write_1(sc->sc_iot,sc->sc_ioh,0xce,0x00);

	val = bus_space_read_2(sc->sc_iot,sc->sc_ioh,0xcc) | (bus_space_read_1(sc->sc_iot,sc->sc_ioh,0xcd) << 8); 
	if(val == 0x55aa){
		printf("\nEsta indexado");
	}

	data = bus_space_read_1(sc->sc_iot,sc->sc_ioh,GMUX_PORT_WRITE);
	if(data){
		gmux_version(sc,GMUX_PORT_VERSION_MAJOR);
       	}
	
	data = bus_space_read_1(sc->sc_iot,sc->sc_ioh,GMUX_PORT_WRITE);
	if(data){
		sc->sc_brightness = gmux_get_brightness(sc);
		printf("\nIluminacion actual: 0x%x",sc->sc_brightness);	
	}
/*
	gmux_set_brightness(sc,0x200A00);
	data = bus_space_read_1(sc->sc_iot,sc->sc_ioh,GMUX_PORT_WRITE);
	if(data){
		printf("\nIluminacion actual: 0x%x",gmux_get_brightness(sc));	
	
	}
	*/

	/* Map wsconsole hook functions. */
	ws_get_param = gmux_get_param;
	ws_set_param = gmux_set_param;

	printf("\n");
}

void gmux_complete(struct gmux_softc *sc){

	int counter = 50;
	uint8_t complete;

	complete = bus_space_read_1(sc->sc_iot,sc->sc_ioh,GMUX_PORT_WRITE);
	while(counter && !(complete & 0x01)){
		complete = bus_space_read_1(sc->sc_iot,sc->sc_ioh,GMUX_PORT_WRITE);
		delay(10);
		counter--;
	}		

	if (complete & 0x01)
		bus_space_read_1(sc->sc_iot,sc->sc_ioh,GMUX_PORT_WRITE);

}

void gmux_version(struct gmux_softc *sc, int port){

	uint32_t version;
	uint8_t ver_major, ver_minor, ver_release;

	bus_space_write_1(sc->sc_iot,sc->sc_ioh,GMUX_PORT_READ,port);
	gmux_complete(sc);
	version = bus_space_read_4(sc->sc_iot,sc->sc_ioh,GMUX_PORT_VALUE);
	ver_major = (version >> 24) & 0xff;
	ver_minor = (version >> 16) & 0xff;
	ver_release = (version >> 8) & 0xff;
	printf("\nVersion de gmux es: %d.%d.%d",ver_major,ver_minor,ver_release);
}

int gmux_get_brightness(struct gmux_softc *sc){
	
	bus_space_write_1(sc->sc_iot,sc->sc_ioh,GMUX_PORT_READ,GMUX_PORT_BRIGHTNESS);
	gmux_complete(sc);
	return bus_space_read_4(sc->sc_iot,sc->sc_ioh,GMUX_PORT_VALUE);	
}

void gmux_set_brightness(struct gmux_softc *sc, uint32_t brightness){
	
	int i;
	uint8_t store_val, data;

	for(i=0;i<4;i++){
		store_val = (brightness >> 8 * i) & 0xff;
		bus_space_write_1(sc->sc_iot,sc->sc_ioh,GMUX_PORT_VALUE + i,store_val);
	}

	data = bus_space_read_1(sc->sc_iot,sc->sc_ioh,GMUX_PORT_WRITE);
	if(data){
		bus_space_write_1(sc->sc_iot,sc->sc_ioh,GMUX_PORT_WRITE,GMUX_PORT_BRIGHTNESS);
		gmux_complete(sc);
	}

}

int gmux_get_param(struct wsdisplay_param *dp)
{
	struct gmux_softc *sc = gmux_cd.cd_devs[0];

	if (sc == NULL)
		return -1;

	switch (dp->param) {
	case WSDISPLAYIO_PARAM_BACKLIGHT:
		printf("gmux_get_param: sc->sc_brightness = %d\n",
		    sc->sc_brightness);
		dp->min = GMUX_MIN_BRIGHTNESS;
		dp->max = GMUX_MAPPED_MAX_BRIGHTNESS;
		dp->curval = map_value(sc->sc_brightness);
		printf("dp->curval es: %d\n",dp->curval);
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
		printf("gmux_set_param: curval = %d\n", dp->curval);
		if (dp->curval < GMUX_MIN_BRIGHTNESS)
			dp->curval = 0;
		if (dp->curval > GMUX_MAPPED_MAX_BRIGHTNESS)
			dp->curval = GMUX_MAPPED_MAX_BRIGHTNESS;
		gmux_set_brightness(sc, inverse_map_value(dp->curval));
		sc->sc_brightness = inverse_map_value(dp->curval);
		return 0;
	default:
		return -1;
	}
}


int map_value(uint32_t brightness){
	return (GMUX_MAPPED_MAX_BRIGHTNESS * (int)(brightness / GMUX_REAL_MAX_BRIGHTNESS));
}

int inverse_map_value(uint32_t brightness) {
	return ((int)(brightness / GMUX_MAPPED_MAX_BRIGHTNESS) * GMUX_REAL_MAX_BRIGHTNESS);
}
