#include <sys/param.h>
#include <sys/systm.h>

#include <dev/acpi/amltypes.h>
#include <dev/acpi/acpivar.h>
//#include <dev/acpi/acpidev.h>
//#include <dev/acpi/dsdt.h>

#ifdef GMUX_DEBUG
#define DPRINTF(x) printf x
#else
#define DPRINTF(x)
#endif

struct gmux_softc {
	struct device		 sc_dev;

	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_devnode;

	bus_space_tag_t		 sc_bt;
	bus_space_handle_t	 sc_bh;

	bus_addr_t		 sc_io_base;
	uint8_t			 sc_brightness;
};

int	gmux_match(struct device *, void *, void *);
void	gmux_attach(struct device *, struct device *, void *);

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

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_devnode = aaa->aaa_node;

	printf(": %s", sc->sc_devnode->name);
}

