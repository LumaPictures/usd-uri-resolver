#pragma once

#include <pxr/usd/sdf/fileFormat.h>
#include <pxr/base/tf/staticTokens.h>

#include <string>
#include <iostream>

#define USDDATABASE_FILE_FORMAT_TOKENS  \
    ((Id,      "db"))                   \
    ((Version, "1.0"))                  \
    ((Target,  "usd"))

TF_DECLARE_PUBLIC_TOKENS(UsdDatabaseFileFormatTokens, USDDATABASE_FILE_FORMAT_TOKENS);

TF_DECLARE_WEAK_AND_REF_PTRS(UsdDatabaseFileFormat);
TF_DECLARE_WEAK_AND_REF_PTRS(SdfLayerBase);

class UsdDatabaseFileFormat : public SdfFileFormat {
public:

    // SdfFileFormat overrides.
    virtual bool CanRead(const std::string &file) const
    {
        std::cerr << "Checking if can read!" << file << std::endl;
        return true;
    }

    virtual bool ReadFromFile(const SdfLayerBasePtr& layerBase,
                              const std::string& filePath,
                              bool metadataOnly) const
    {
        std::cerr << "Trying to read from file : " << filePath << std::endl;
        return false;
    }

    virtual bool ReadFromString(const SdfLayerBasePtr& layerBase,
                                const std::string& str) const
    {
        return false;
    }

    virtual bool WriteToString(const SdfLayerBase* layerBase,
                               std::string* str,
                               const std::string& comment=std::string()) const
    {
        return false;
    }

    virtual bool WriteToStream(const SdfSpecHandle &spec,
                               std::ostream& out,
                               size_t indent) const
    {
        return false;
    }

protected:
    SDF_FILE_FORMAT_FACTORY_ACCESS;

    virtual bool _IsStreamingLayer(const SdfLayerBase& layer) const
    {
        return false;
    }

    virtual ~UsdDatabaseFileFormat()
    {

    }

    UsdDatabaseFileFormat();
};
