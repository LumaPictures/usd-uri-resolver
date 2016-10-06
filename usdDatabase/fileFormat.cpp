#include "fileFormat.h"

#include <pxr/base/tf/fileUtils.h>
#include <pxr/base/tf/registryManager.h>

__attribute__((constructor))
static void load_test()
{
    std::cerr << "Loading plugin!" << std::endl;
}

TF_DEFINE_PUBLIC_TOKENS(
    UsdDatabaseFileFormatTokens,
    USDDATABASE_FILE_FORMAT_TOKENS);

TF_REGISTRY_FUNCTION(TfType)
{
    std::cerr << "Yay!" << std::endl;
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

