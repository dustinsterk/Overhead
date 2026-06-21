// Single definition point for the large generated asset arrays.
//
// StarCatalog.h / Coastline.h used to define their big tables as file-`static const`,
// so EVERY .cpp that included them got its own private copy — nm showed kStars x2,
// kCoastline x3, kConLines x2, wasting ~60 KB of flash (which pushed the image right
// up against the 1.75 MB app slot and made it unbootable). The headers now guard the
// real definitions behind OVERHEAD_ASSETS_IMPL and otherwise declare them `extern`;
// this is the one translation unit that emits them.
#define OVERHEAD_ASSETS_IMPL
#include "StarCatalog.h"
#include "Coastline.h"
