/*
Abstract:
  Providers enumerator implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

#include "../error_codes.h"

#include "enumerator.h"

#include <error.h>

#include <algorithm>
#include <boost/bind.hpp>
#include <boost/bind/apply.hpp>

#include <text/io.h>

#define FILE_TAG 03113EE3

namespace
{
  using namespace ZXTune::IO;
  
  inline bool CompareInfos(const ProviderInfo& lh, const ProviderInfo& rh)
  {
    return lh.Name == rh.Name;
  }
  
  class ProvidersEnumeratorImpl : public ProvidersEnumerator
  {
    struct ProviderEntry
    {
      ProviderEntry() : Checker(), Opener(), Splitter(), Combiner()
      {
      }
      
      ProviderEntry(const ProviderInfo& info, 
        Provider::CheckFunc checker, Provider::OpenFunc opener, Provider::SplitFunc splitter, Provider::CombineFunc combiner)
	: Info(info), Checker(checker), Opener(opener), Splitter(splitter), Combiner(combiner)
      {
      }
      ProviderInfo Info;
      Provider::CheckFunc Checker;
      Provider::OpenFunc Opener;
      Provider::SplitFunc Splitter;
      Provider::CombineFunc Combiner;
    };
    typedef std::list<ProviderEntry> ProvidersList;
  public:
    ProvidersEnumeratorImpl()
    {
    }
    
    virtual void RegisterProvider(const ProviderInfo& info,
	Provider::CheckFunc detector, Provider::OpenFunc opener, Provider::SplitFunc splitter, Provider::CombineFunc combiner)
    {
      assert(Providers.end() == std::find_if(Providers.begin(), Providers.end(), 
        boost::bind(CompareInfos, info, boost::bind<ProviderInfo>(&ProviderEntry::Info, _1))));
      Providers.push_back(ProviderEntry(info, detector, opener, splitter, combiner));
    }
      
    virtual Error OpenUri(const String& uri, const OpenDataParameters& params, DataContainer::Ptr& result, String& subpath) const
    {
      const ProvidersList::const_iterator it = FindProvider(uri);
      if (it != Providers.end())
      {
        return it->Opener(uri, params, result, subpath);
      }
      return Error(THIS_LINE, NOT_SUPPORTED, TEXT_IO_ERROR_NOT_SUPPORTED_URI);
    }
    
    virtual Error SplitUri(const String& uri, String& baseUri, String& subpath) const
    {
      const ProvidersList::const_iterator it = FindProvider(uri);
      if (it != Providers.end())
      {
        return it->Splitter(uri, baseUri, subpath);
      }
      return Error(THIS_LINE, NOT_SUPPORTED, TEXT_IO_ERROR_NOT_SUPPORTED_URI);
    }
    
    virtual Error CombineUri(const String& baseUri, const String& subpath, String& uri) const
    {
      const ProvidersList::const_iterator it = FindProvider(uri);
      if (it != Providers.end())
      {
        return it->Combiner(baseUri, subpath, uri);
      }
      return Error(THIS_LINE, NOT_SUPPORTED, TEXT_IO_ERROR_NOT_SUPPORTED_URI);
    }
      
    virtual void Enumerate(std::vector<ProviderInfo>& infos) const
    {
      std::vector<ProviderInfo> result(Providers.size());
      std::transform(Providers.begin(), Providers.end(), result.begin(), boost::bind<ProviderInfo>(&ProviderEntry::Info, _1));
      infos.swap(result);
    }
  private:
    ProvidersList::const_iterator FindProvider(const String& uri) const
    {
      return std::find_if(Providers.begin(), Providers.end(), boost::bind(boost::apply<bool>(), boost::bind(&ProviderEntry::Checker, _1), uri));
    }
  private:
    ProvidersList Providers;
  };
}

namespace ZXTune
{
  namespace IO
  {
    ProvidersEnumerator& ProvidersEnumerator::Instance()
    {
      static ProvidersEnumeratorImpl instance;
      return instance;
    }
    
    namespace Provider
    {
      AutoRegistrator::AutoRegistrator(const ProviderInfo& info, 
        CheckFunc checker, OpenFunc opener, SplitFunc splitter, CombineFunc combiner)
      {
        ProvidersEnumerator::Instance().RegisterProvider(info, checker, opener, splitter, combiner);
      }
    }
    
    Error OpenData(const String& uri, const OpenDataParameters& params, DataContainer::Ptr& data, String& subpath)
    {
      return ProvidersEnumerator::Instance().OpenUri(uri, params, data, subpath);
    }
    
    Error SplitUri(const String& uri, String& baseUri, String& subpath)
    {
      return ProvidersEnumerator::Instance().SplitUri(uri, baseUri, subpath);
    }
    
    Error CombineUri(const String& baseUri, const String& subpath, String& uri)
    {
      return ProvidersEnumerator::Instance().CombineUri(baseUri, subpath, uri);
    }
    
    void GetSupportedProviders(std::vector<ProviderInfo>& infos)
    {
      return ProvidersEnumerator::Instance().Enumerate(infos);
    }
  }
}
