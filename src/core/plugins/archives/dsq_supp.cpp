/*
Abstract:
  DSQ convertors support

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "extraction_result.h"
#include <core/plugins/registrator.h>
//library includes
#include <core/plugin_attrs.h>
#include <formats/packed_decoders.h>
#include <io/container.h>
//boost includes
#include <boost/enable_shared_from_this.hpp>
//text includes
#include <core/text/plugins.h>

namespace
{
  using namespace ZXTune;

  const Char DSQ_PLUGIN_ID[] = {'D', 'S', 'Q', '\0'};
  const String DSQ_PLUGIN_VERSION(FromStdString("$Rev$"));

  class DSQPlugin : public ArchivePlugin
                  , public boost::enable_shared_from_this<DSQPlugin>
  {
  public:
    DSQPlugin()
      : Decoder(Formats::Packed::CreateDataSquieezerDecoder())
    {
    }

    virtual String Id() const
    {
      return DSQ_PLUGIN_ID;
    }

    virtual String Description() const
    {
      return Text::DSQ_PLUGIN_INFO;
    }

    virtual String Version() const
    {
      return DSQ_PLUGIN_VERSION;
    }

    virtual uint_t Capabilities() const
    {
      return CAP_STOR_CONTAINER;
    }

    virtual DetectionResult::Ptr Detect(DataLocation::Ptr inputData, const Module::DetectCallback& callback) const
    {
      return DetectModulesInArchive(shared_from_this(), *Decoder, inputData, callback);
    }

    virtual DataLocation::Ptr Open(const Parameters::Accessor& /*parameters*/,
                                   DataLocation::Ptr inputData,
                                   const DataPath& pathToOpen) const
    {
      return OpenDataFromArchive(shared_from_this(), *Decoder, inputData, pathToOpen);
    }
  private:
    const Formats::Packed::Decoder::Ptr Decoder;
  };
}

namespace ZXTune
{
  void RegisterDSQConvertor(PluginsRegistrator& registrator)
  {
    const ArchivePlugin::Ptr plugin(new DSQPlugin());
    registrator.RegisterPlugin(plugin);
  }
}
