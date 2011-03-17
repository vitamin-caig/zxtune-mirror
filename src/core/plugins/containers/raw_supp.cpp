/*
Abstract:
  RAW containers support

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include <core/plugins/callback.h>
#include <core/plugins/registrator.h>
#include <core/plugins/utils.h>
//common includes
#include <error_tools.h>
#include <logging.h>
#include <tools.h>
//library includes
#include <core/error_codes.h>
#include <core/module_detect.h>
#include <core/plugin_attrs.h>
#include <core/plugins_parameters.h>
#include <io/container.h>
#include <io/fs_tools.h>
//boost includes
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
//text includes
#include <core/text/core.h>
#include <core/text/plugins.h>

#define FILE_TAG 7E0CBD98

namespace
{
  using namespace ZXTune;

  const Char RAW_PLUGIN_ID[] = {'R', 'A', 'W', 0};
  const String RAW_PLUGIN_VERSION(FromStdString("$Rev$"));

  const uint_t MIN_SCAN_STEP = 1;
  const uint_t MAX_SCAN_STEP = 256;
  const std::size_t MIN_MINIMAL_RAW_SIZE = 128;

  //fake parameter name used to prevent plugin recursive call while first pass processing
  const Char RAW_PLUGIN_RECURSIVE_DEPTH[] =
  {
    'r','a','w','_','s','c','a','n','e','r','_','r','e','c','u','r','s','i','o','n','_','d','e','p','t','h','\0'
  };

  const Char RAW_PREFIX[] = {'+', 0};

  //\+\d+
  inline bool CheckIfRawPart(const String& str, std::size_t& offset)
  {
    static const String PREFIX(RAW_PREFIX);

    Parameters::IntType res = 0;
    if (!str.empty() && 
        0 == str.find(PREFIX) &&
        Parameters::ConvertFromString(str.substr(PREFIX.size()), res))
    {
      offset = res;
      return true;
    }
    return false;
  }

  inline String CreateRawPart(std::size_t offset)
  {
    String res(RAW_PREFIX);
    res += Parameters::ConvertToString(offset);
    return res;
  }

  class RawPluginParameters
  {
  public:
    explicit RawPluginParameters(const Parameters::Accessor& accessor)
      : Accessor(accessor)
    {
    }

    int_t GetRecursiveDepth() const
    {
      Parameters::IntType depth = -1;
      Accessor.FindIntValue(RAW_PLUGIN_RECURSIVE_DEPTH, depth);
      return static_cast<int_t>(depth);
    }

    std::size_t GetMinimalSize() const
    {
      Parameters::IntType minRawSize = Parameters::ZXTune::Core::Plugins::Raw::MIN_SIZE_DEFAULT;
      if (Accessor.FindIntValue(Parameters::ZXTune::Core::Plugins::Raw::MIN_SIZE, minRawSize) &&
          minRawSize < Parameters::IntType(MIN_MINIMAL_RAW_SIZE))
      {
        throw MakeFormattedError(THIS_LINE, Module::ERROR_INVALID_PARAMETERS,
          Text::RAW_ERROR_INVALID_MIN_SIZE, minRawSize, MIN_MINIMAL_RAW_SIZE);
      }
      return static_cast<std::size_t>(minRawSize);
    }

    std::size_t GetScanStep() const
    {
      Parameters::IntType scanStep = Parameters::ZXTune::Core::Plugins::Raw::SCAN_STEP_DEFAULT;
      if (Accessor.FindIntValue(Parameters::ZXTune::Core::Plugins::Raw::SCAN_STEP, scanStep) &&
          (scanStep < Parameters::IntType(MIN_SCAN_STEP) ||
           scanStep > Parameters::IntType(MAX_SCAN_STEP)))
      {
        throw MakeFormattedError(THIS_LINE, Module::ERROR_INVALID_PARAMETERS,
          Text::RAW_ERROR_INVALID_STEP, scanStep, MIN_SCAN_STEP, MAX_SCAN_STEP);
      }
      return static_cast<std::size_t>(scanStep);
    }
  private:
    const Parameters::Accessor& Accessor;
  };

  class DepthLimitedParameters : public Parameters::Accessor
  {
  public:
    DepthLimitedParameters(Parameters::Accessor::Ptr delegate, std::size_t depth)
      : Delegate(delegate)
      , Depth(depth)
    {
    }

    virtual bool FindIntValue(const Parameters::NameType& name, Parameters::IntType& val) const
    {
      if (name == RAW_PLUGIN_RECURSIVE_DEPTH)
      {
        val = Depth;
        return true;
      }
      else
      {
        return Delegate->FindIntValue(name, val);
      }
    }

    virtual bool FindStringValue(const Parameters::NameType& name, Parameters::StringType& val) const
    {
      return Delegate->FindStringValue(name, val);
    }

    virtual bool FindDataValue(const Parameters::NameType& name, Parameters::DataType& val) const
    {
      return Delegate->FindDataValue(name, val);
    }

    virtual void Process(Parameters::Visitor& visitor) const
    {
      return Delegate->Process(visitor);
    }
  private:
    const Parameters::Accessor::Ptr Delegate;
    const std::size_t Depth;
  };

  class RawProgressCallback : public Log::ProgressCallback
  {
  public:
    RawProgressCallback(const Module::DetectCallback& callback, uint_t limit, const String& path)
      : Delegate(CreateProgressCallback(callback, limit))
      , Text(path.empty() ? String(Text::PLUGIN_RAW_PROGRESS_NOPATH) : (Formatter(Text::PLUGIN_RAW_PROGRESS) % path).str())
    {
    }

    virtual void OnProgress(uint_t current)
    {
      OnProgress(current, Text);
    }

    virtual void OnProgress(uint_t current, const String& message)
    {
      if (Delegate.get())
      {
        Delegate->OnProgress(current, message);
      }
    }
  private:
    const Log::ProgressCallback::Ptr Delegate;
    const String Text;
  };

  class ScanDataContainer : public IO::DataContainer
  {
  public:
    typedef boost::shared_ptr<ScanDataContainer> Ptr;

    ScanDataContainer(IO::DataContainer::Ptr delegate, std::size_t offset)
      : Delegate(delegate)
      , OriginalSize(delegate->Size())
      , OriginalData(static_cast<const uint8_t*>(delegate->Data()))
      , Offset(offset)
    {
    }

    virtual std::size_t Size() const
    {
      return OriginalSize - Offset;
    }

    virtual const void* Data() const
    {
      return OriginalData + Offset;
    }

    virtual IO::DataContainer::Ptr GetSubcontainer(std::size_t offset, std::size_t size) const
    {
      return Delegate->GetSubcontainer(offset + Offset, size);
    }

    std::size_t GetOffset()
    {
      return Offset;
    }

    void Move(std::size_t step)
    {
      Offset += step;
    }
  private:
    const IO::DataContainer::Ptr Delegate;
    const std::size_t OriginalSize;
    const uint8_t* const OriginalData;
    std::size_t Offset;
  };

  class ScanModuleContainer : public Module::Container
  {
  public:
    typedef boost::shared_ptr<ScanModuleContainer> Ptr;

    ScanModuleContainer(Module::Container::Ptr parent, Plugin::Ptr subPlugin, std::size_t offset)
      : Parent(parent)
      , Subdata(boost::make_shared<ScanDataContainer>(Parent->GetData(), offset))
      , Subplugin(subPlugin)
    {
    }

    virtual IO::DataContainer::Ptr GetData() const
    {
      return Subdata;
    }

    virtual String GetPath() const
    {
      const String subPath = CreateRawPart(Subdata->GetOffset());
      return IO::AppendPath(Parent->GetPath(), subPath);
    }

    virtual PluginsChain::ConstPtr GetPlugins() const
    {
      const PluginsChain::Ptr result = Parent->GetPlugins()->Clone();
      result->Add(Subplugin);
      return result;
    }

    bool HasToScan(std::size_t minSize) const
    {
      return Subdata->Size() >= minSize;
    }

    std::size_t GetOffset()
    {
      return Subdata->GetOffset();
    }

    void Move(std::size_t step)
    {
      return Subdata->Move(step);
    }
  private:
    const Module::Container::Ptr Parent;
    const ScanDataContainer::Ptr Subdata;
    const Plugin::Ptr Subplugin;
  };

  class NewParamsCallback : public Module::DetectCallbackDelegate
  {
  public:
    NewParamsCallback(const Module::DetectCallback& delegate, Parameters::Accessor::Ptr plugParams)
      : DetectCallbackDelegate(delegate)
      , PluginsParams(plugParams)
    {
    }

    virtual Parameters::Accessor::Ptr GetPluginsParameters() const
    {
      return PluginsParams;
    }
  private:
    const Parameters::Accessor::Ptr PluginsParams;
  };

  bool CheckIfLastIsScanner(const PluginsChain& plugins)
  {
    if (Plugin::Ptr lastPlugin = plugins.GetLast())
    {
      return lastPlugin->Id() == RAW_PLUGIN_ID;
    }
    return false;
  }

  class RawScaner : public ContainerPlugin
                  , public boost::enable_shared_from_this<RawScaner>
  {
  public:
    virtual String Id() const
    {
      return RAW_PLUGIN_ID;
    }

    virtual String Description() const
    {
      return Text::RAW_PLUGIN_INFO;
    }

    virtual String Version() const
    {
      return RAW_PLUGIN_VERSION;
    }

    virtual uint_t Capabilities() const
    {
      return CAP_STOR_MULTITRACK | CAP_STOR_SCANER;
    }

    virtual bool Check(const IO::DataContainer& inputData) const
    {
      //check only size restrictions
      return inputData.Size() >= MIN_MINIMAL_RAW_SIZE;
    }

    virtual std::size_t Process(Module::Container::Ptr container, const Module::DetectCallback& callback) const
    {
      //do not search right after previous raw plugin
      const PluginsChain::ConstPtr plugins = container->GetPlugins();
      if (CheckIfLastIsScanner(*plugins))
      {
        return 0;
      }

      const Parameters::Accessor::Ptr pluginParams = callback.GetPluginsParameters();
      RawPluginParameters scanParams(*pluginParams);
      //special mark to determine if plugin is called due to recursive scan
      const uint_t pluginsInChain = plugins->Count();
      if (scanParams.GetRecursiveDepth() == int_t(pluginsInChain))
      {
        return 0;
      }
      const IO::DataContainer::Ptr data = container->GetData();
      const std::size_t limit = data->Size();
      //process without offset
      const Parameters::Accessor::Ptr paramsForCheckAtBegin =
        boost::make_shared<DepthLimitedParameters>(pluginParams, pluginsInChain);
      const NewParamsCallback newCallback(callback, paramsForCheckAtBegin);
      const std::size_t dataUsedAtBegin = Module::DetectModules(container, newCallback);

      const std::size_t minRawSize = scanParams.GetMinimalSize();

      //check for further scanning possibility
      if (dataUsedAtBegin + minRawSize >= limit)
      {
        return limit;
      }

      const std::size_t scanStep = scanParams.GetScanStep();

      //to determine was scaner really affected
      bool wasResult = dataUsedAtBegin != 0;


      const ContainerPlugin::Ptr subPlugin = shared_from_this();
      const std::size_t startOffset = std::max(dataUsedAtBegin, scanStep);
      const ScanModuleContainer::Ptr subContainer = boost::make_shared<ScanModuleContainer>(container, subPlugin, startOffset);

      const Log::ProgressCallback::Ptr progress(new RawProgressCallback(callback, limit, container->GetPath()));
      const Module::NoProgressDetectCallback noProgressCallback(callback);
      while (subContainer->HasToScan(minRawSize))
      {
        const std::size_t offset = subContainer->GetOffset();
        progress->OnProgress(offset);
        const std::size_t usedSize = Module::DetectModules(subContainer, noProgressCallback);
        wasResult = wasResult || usedSize != 0;
        subContainer->Move(std::max(usedSize, scanStep));
      }
      return wasResult
        ? limit
        : 0;
    }

    IO::DataContainer::Ptr Open(const Parameters::Accessor& /*commonParams*/,
      const MetaContainer& inData, const String& inPath, String& restPath) const
    {
      //do not open right after self
      if (CheckIfLastIsScanner(*inData.Plugins))
      {
        return IO::DataContainer::Ptr();
      }
      String restComp;
      const String& pathComp = IO::ExtractFirstPathComponent(inPath, restComp);
      std::size_t offset = 0;
      if (CheckIfRawPart(pathComp, offset))
      {
        restPath = restComp;
        return inData.Data->GetSubcontainer(offset, inData.Data->Size() - offset);
      }
      return IO::DataContainer::Ptr();
    }
  };
}

namespace ZXTune
{
  void RegisterRawContainer(PluginsRegistrator& registrator)
  {
    const ContainerPlugin::Ptr plugin(new RawScaner());
    registrator.RegisterPlugin(plugin);
  }
}
