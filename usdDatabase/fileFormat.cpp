#include "fileFormat.h"

#include <pxr/base/tf/fileUtils.h>
#include <pxr/base/tf/registryManager.h>

TF_DEFINE_PUBLIC_TOKENS(
    UsdDatabaseFileFormatTokens,
    USDDATABASE_FILE_FORMAT_TOKENS);

TF_REGISTRY_FUNCTION(TfType)
{
    SDF_DEFINE_FILE_FORMAT(UsdDatabaseFileFormat, SdfFileFormat);
}

UsdDatabaseFileFormat::UsdDatabaseFileFormat()
    : SdfFileFormat(
    UsdDatabaseFileFormatTokens->Id,
    UsdDatabaseFileFormatTokens->Version,
    UsdDatabaseFileFormatTokens->Target,
    UsdDatabaseFileFormatTokens->Id)
{
}

