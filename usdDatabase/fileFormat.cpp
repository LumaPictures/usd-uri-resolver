#include "fileFormat.h"

#include <pxr/base/tf/fileUtils.h>
#include <pxr/base/tf/registryManager.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/sphere.h>

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

UsdDatabaseFileFormat::~UsdDatabaseFileFormat()
{

}

bool UsdDatabaseFileFormat::CanRead(const std::string& file) const
{
    return true;
}

bool UsdDatabaseFileFormat::ReadFromFile(const SdfLayerBasePtr& layerBase, const std::string& filePath,
                                         bool metadataOnly) const
{
    SdfLayerHandle layer = TfDynamic_cast<SdfLayerHandle>(layerBase);
    if (!TF_VERIFY(layer))
        return false;

    SdfLayerRefPtr new_layer = SdfLayer::CreateAnonymous(".usda");
    UsdStageRefPtr stage = UsdStage::Open(new_layer);
    UsdGeomSphere xform = UsdGeomSphere::Define(stage, SdfPath("/my_xform"));
    layer->TransferContent(new_layer);
    return true;
}

bool UsdDatabaseFileFormat::ReadFromString(const SdfLayerBasePtr& layerBase, const std::string& str) const
{
    return false;
}

bool
UsdDatabaseFileFormat::WriteToString(const SdfLayerBase* layerBase, std::string* str, const std::string& comment) const
{
    return false;
}

bool UsdDatabaseFileFormat::WriteToStream(const SdfSpecHandle& spec, std::ostream& out, size_t indent) const
{
    return false;
}

bool UsdDatabaseFileFormat::_IsStreamingLayer(const SdfLayerBase& layer) const
{
    return false;
}
