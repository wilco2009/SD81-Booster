#ifndef VERSION_CHECK_H
#define VERSION_CHECK_H

#include <Arduino.h>

// Codeberg protege TODO el dominio codeberg.org (API, descargas de release,
// vistas "raw", el feed RSS, e incluso peticiones fetch() lanzadas desde un
// navegador real) con un sistema anti-bot que bloquea cualquier cliente que
// no sea una navegacion de pagina completa - probado a mano en esta misma
// sesion contra curl, PowerShell y el propio Chrome via fetch(), los tres
// bloqueados por igual (ver memoria del proyecto). GitHub, en cambio, no
// tiene ese bloqueo - curl a la API y a la descarga real de un asset
// funcionan sin pegas. OJO: la API (api.github.com) SI lleva
// Access-Control-Allow-Origin: *, pero el blob real del asset
// (release-assets.githubusercontent.com, donde redirige la URL de
// descarga) NO lleva esa cabecera - un fetch() cruzado desde el navegador
// a esa URL falla con CORS ("Failed to fetch"). Por eso la descarga del
// ZIP desde el navegador pasa por el proxy /proxy del propio ESP32 (ver
// Wifi_module_01.ino) en vez de hacer fetch() directo a GitHub.
//
// Por eso la comprobacion/descarga de actualizaciones consulta un MIRROR en
// GitHub del repo, no Codeberg directamente. Codeberg sigue siendo el repo
// principal para todo lo demas (desarrollo, issues, etc.) - el mirror en
// GitHub solo necesita tener las releases publicadas (mismos tags, mismos
// assets) para que esto funcione.
//
#define GITHUB_OWNER "wilco2009"
#define GITHUB_REPO  "SD81-Booster"

struct ReleaseInfo {
  String tag;                // ej. "v1.1.0"
  String firmware_zip_url;   // asset cuyo nombre contiene "Firmware"
  String sdcontent_zip_url;  // asset cuyo nombre contiene "SD Content"
};

// Consulta la API de GitHub (releases/latest del mirror) para la ultima
// release publicada. Devuelve false si la peticion HTTPS falla, el JSON no
// se puede parsear, o no se encuentran los dos assets esperados.
bool version_check_fetch_latest(ReleaseInfo* out);

// Lee /SYS/VERSION.TXT de la SD (lo trae cada paquete de release, ver
// memoria del proyecto). Cadena vacia si el fichero no existe - pasa con
// cualquier instalacion anterior a que existiera este mecanismo (p.ej.
// v1.0.0), y se trata como "version desconocida, hay actualizacion" al
// comparar.
String version_check_get_installed();

#endif
