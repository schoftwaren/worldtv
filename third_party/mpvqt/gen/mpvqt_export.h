// Minimal replacement for the ecm_generate_export_header output of upstream
// MpvQt. The vendored build is static-only; the shim target defines
// MPVQT_STATIC_BUILD. Only MPVQT_EXPORT is referenced by the sources.
#pragma once

#define MPVQT_EXPORT
#define MPVQT_NO_EXPORT
