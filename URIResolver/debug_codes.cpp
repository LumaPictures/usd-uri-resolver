#include "debug_codes.h"

#include <pxr/base/tf/debug.h>
#include <pxr/base/tf/registryManager.h>
#include <pxr/pxr.h>

PXR_NAMESPACE_OPEN_SCOPE

TF_REGISTRY_FUNCTION(TfDebug) {
    TF_DEBUG_ENVIRONMENT_SYMBOL(
        USD_URI_RESOLVER, "Usd URI handling asset resolver.");

    TF_DEBUG_ENVIRONMENT_SYMBOL(
        USD_URI_SQL_RESOLVER, "Usd SQL asset resolver.");
}

PXR_NAMESPACE_CLOSE_SCOPE
