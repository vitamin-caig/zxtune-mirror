/*
Abstract:
  AYM-based players common functionality implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "ay_base.h"
//library includes
#include <core/error_codes.h>
//std includes
#include <limits>
//boost includes
#include <boost/bind.hpp>
#include <boost/mem_fn.hpp>
//text includes
#include <core/text/core.h>

#define FILE_TAG 7D6A549C

namespace
{
  using namespace ZXTune;
  using namespace ZXTune::Module;

  class AYMReceiver : public Devices::AYM::Receiver
  {
  public:
    explicit AYMReceiver(Sound::MultichannelReceiver::Ptr target)
      : Target(target)
      , Data(Devices::AYM::CHANNELS)
    {
    }

    virtual void ApplyData(const Devices::AYM::MultiSample& data)
    {
      std::transform(data.begin(), data.end(), Data.begin(), &AYMSampleToSoundSample);
      Target->ApplyData(Data);
    }

    virtual void Flush()
    {
      Target->Flush();
    }
  private:
    inline static Sound::Sample AYMSampleToSoundSample(Devices::AYM::Sample in)
    {
      return in / 2;
    }
  private:
    const Sound::MultichannelReceiver::Ptr Target;
    std::vector<Sound::Sample> Data;
  };

  class AYMAnalyzer : public Analyzer
  {
  public:
    explicit AYMAnalyzer(Devices::AYM::Chip::Ptr device)
      : Device(device)
    {
    }

    //analyzer virtuals
    virtual uint_t ActiveChannels() const
    {
      Devices::AYM::ChannelsState state;
      Device->GetState(state);
      return static_cast<uint_t>(std::count_if(state.begin(), state.end(),
        boost::mem_fn(&Devices::AYM::ChanState::Enabled)));
    }

    virtual void BandLevels(std::vector<std::pair<uint_t, uint_t> >& bandLevels) const
    {
      Devices::AYM::ChannelsState state;
      Device->GetState(state);
      bandLevels.resize(state.size());
      std::transform(state.begin(), state.end(), bandLevels.begin(),
        boost::bind(&std::make_pair<uint_t, uint_t>, boost::bind(&Devices::AYM::ChanState::Band, _1), boost::bind(&Devices::AYM::ChanState::LevelInPercents, _1)));
    }
  private:
    const Devices::AYM::Chip::Ptr Device;
  };

  class AYMRenderer : public Renderer
  {
  public:
    AYMRenderer(AYM::ParametersHelper::Ptr params, StateIterator::Ptr iterator, AYMDataRenderer::Ptr renderer, Devices::AYM::Chip::Ptr device)
      : Renderer(renderer)
      , Device(device)
      , AYMHelper(params)
      , Iterator(iterator)
    {
#ifndef NDEBUG
//perform self-test
      Reset();
      Devices::AYM::DataChunk chunk;
      const AYM::TrackBuilder track(AYMHelper->GetFreqTable(), chunk);
      do
      {
        Renderer->SynthesizeData(*Iterator, track);
      }
      while (Iterator->NextFrame(0, false));
#endif
      Reset();
    }

    virtual TrackState::Ptr GetTrackState() const
    {
      return Iterator;
    }

    virtual Analyzer::Ptr GetAnalyzer() const
    {
      return boost::make_shared<AYMAnalyzer>(Device);
    }

    virtual bool RenderFrame(const Sound::RenderParameters& params)
    {
      const uint64_t ticksDelta = params.ClocksPerFrame();

      Devices::AYM::DataChunk chunk;
      AYMHelper->GetDataChunk(chunk);
      chunk.Tick = Iterator->AbsoluteTick() + ticksDelta;
      const AYM::TrackBuilder track(AYMHelper->GetFreqTable(), chunk);

      Renderer->SynthesizeData(*Iterator, track);

      const bool res = Iterator->NextFrame(ticksDelta, params.Looped());
      Device->RenderData(params, chunk);
      return res;
    }

    virtual void Reset()
    {
      Device->Reset();
      Iterator->Reset();
      Renderer->Reset();
    }

    virtual void SetPosition(uint_t frame)
    {
      if (frame < Iterator->Frame())
      {
        //reset to beginning in case of moving back
        Iterator->Seek(0);
        Renderer->Reset();
      }
      //fast forward
      Devices::AYM::DataChunk chunk;
      const AYM::TrackBuilder track(AYMHelper->GetFreqTable(), chunk);
      while (Iterator->Frame() < frame)
      {
        //do not update tick for proper rendering
        Renderer->SynthesizeData(*Iterator, track);
        if (!Iterator->NextFrame(0, false))
        {
          break;
        }
      }
    }
  private:
    const AYMDataRenderer::Ptr Renderer;
    const Devices::AYM::Chip::Ptr Device;
    const AYM::ParametersHelper::Ptr AYMHelper;
    const StateIterator::Ptr Iterator;
  };
}

namespace ZXTune
{
  namespace Module
  {
    Devices::AYM::Receiver::Ptr CreateAYMReceiver(Sound::MultichannelReceiver::Ptr target)
    {
      return boost::make_shared<AYMReceiver>(target);
    }

    Renderer::Ptr CreateAYMRenderer(AYM::ParametersHelper::Ptr params, StateIterator::Ptr iterator, AYMDataRenderer::Ptr renderer, Devices::AYM::Chip::Ptr device)
    {
      return boost::make_shared<AYMRenderer>(params, iterator, renderer, device);
    }

    Renderer::Ptr CreateAYMStreamRenderer(Parameters::Accessor::Ptr params, Information::Ptr info, AYMDataRenderer::Ptr renderer, Devices::AYM::Chip::Ptr device)
    {
      const AYM::ParametersHelper::Ptr ayParams = AYM::ParametersHelper::Create();
      ayParams->SetParameters(*params);
      const StateIterator::Ptr iterator = CreateStreamStateIterator(info);
      return CreateAYMRenderer(ayParams, iterator, renderer, device);
    }

    Renderer::Ptr CreateAYMTrackRenderer(Parameters::Accessor::Ptr params, Information::Ptr info, TrackModuleData::Ptr data, 
      AYMDataRenderer::Ptr renderer, Devices::AYM::Chip::Ptr device)
    {
      const AYM::ParametersHelper::Ptr ayParams = AYM::ParametersHelper::Create();
      ayParams->SetParameters(*params);
      const StateIterator::Ptr iterator = CreateTrackStateIterator(info, data);
      return CreateAYMRenderer(ayParams, iterator, renderer, device);
    }
  }
}
