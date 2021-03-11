/**
 *
 * @file
 *
 * @brief  SID support plugin
 *
 * @author vitamin.caig@gmail.com
 *
 **/

// local includes
#include "core/plugins/player_plugins_registrator.h"
#include "core/plugins/players/plugin.h"
#include "core/plugins/players/sid/roms.h"
#include "core/plugins/players/sid/songlengths.h"
// common includes
#include <contract.h>
#include <make_ptr.h>
// library includes
#include <core/core_parameters.h>
#include <core/plugin_attrs.h>
#include <core/plugins_parameters.h>
#include <debug/log.h>
#include <devices/details/analysis_map.h>
#include <formats/chiptune/container.h>
#include <formats/chiptune/emulation/sid.h>
#include <module/attributes.h>
#include <module/players/duration.h>
#include <module/players/properties_helper.h>
#include <module/players/streaming.h>
#include <parameters/tracking_helper.h>
#include <strings/encoding.h>
#include <strings/trim.h>
// 3rdparty includes
#include <3rdparty/sidplayfp/builders/resid-builder/resid.h>
#include <3rdparty/sidplayfp/sidplayfp/SidInfo.h>
#include <3rdparty/sidplayfp/sidplayfp/SidTune.h>
#include <3rdparty/sidplayfp/sidplayfp/SidTuneInfo.h>
#include <3rdparty/sidplayfp/sidplayfp/sidplayfp.h>
// boost includes
#include <boost/algorithm/string/predicate.hpp>
// text includes
#include <module/text/platforms.h>

namespace Module::Sid
{
  const Debug::Stream Dbg("Core::SIDSupp");

  void CheckSidplayError(bool ok)
  {
    Require(ok);  // TODO
  }

  class Model : public SidTune
  {
  public:
    using Ptr = std::shared_ptr<Model>;

    explicit Model(Binary::View data)
      : SidTune(static_cast<const uint_least8_t*>(data.Start()), data.Size())
      , Index(selectSong(0))
    {
      CheckSidplayError(getStatus());
    }

    void FillDuration(const Parameters::Accessor& params)
    {
      const auto* md5 = createMD5();
      Duration = GetSongLength(md5, Index - 1);
      if (!Duration)
      {
        Duration = GetDefaultDuration(params);
      }
      Dbg("Duration for %1%/%2% is %3%ms", md5, Index, Duration.Get());
    }

    Time::Milliseconds GetDuration() const
    {
      return Duration;
    }

  private:
    uint_t Index = 0;
    Time::Milliseconds Duration;
  };

  inline const uint8_t* GetData(const Parameters::DataType& dump, const uint8_t* defVal)
  {
    return dump.empty() ? defVal : dump.data();
  }

  /*
   * Interpolation modes
   * 0 - fast sampling+interpolate
   * 1 - regular sampling+interpolate
   * 2 - regular sampling+interpolate+resample
   */

  class SidParameters
  {
  public:
    using Ptr = std::unique_ptr<const SidParameters>;

    explicit SidParameters(Parameters::Accessor::Ptr params)
      : Params(std::move(params))
    {}

    uint_t Version() const
    {
      return Params->Version();
    }

    bool GetFastSampling() const
    {
      return Parameters::ZXTune::Core::SID::INTERPOLATION_NONE == GetInterpolation();
    }

    SidConfig::sampling_method_t GetSamplingMethod() const
    {
      return Parameters::ZXTune::Core::SID::INTERPOLATION_HQ == GetInterpolation() ? SidConfig::RESAMPLE_INTERPOLATE
                                                                                   : SidConfig::INTERPOLATE;
    }

    bool GetUseFilter() const
    {
      Parameters::IntType val = Parameters::ZXTune::Core::SID::FILTER_DEFAULT;
      Params->FindValue(Parameters::ZXTune::Core::SID::FILTER, val);
      return static_cast<bool>(val);
    }

  private:
    Parameters::IntType GetInterpolation() const
    {
      Parameters::IntType val = Parameters::ZXTune::Core::SID::INTERPOLATION_DEFAULT;
      Params->FindValue(Parameters::ZXTune::Core::SID::INTERPOLATION, val);
      return val;
    }

  private:
    const Parameters::Accessor::Ptr Params;
  };

  class SidEngine : public Module::Analyzer
  {
  public:
    using Ptr = std::shared_ptr<SidEngine>;

    SidEngine()
      : Builder("resid")
      , Config(Player.config())
      , UseFilter()
    {}

    void Init(uint_t samplerate, const Parameters::Accessor& params)
    {
      Parameters::DataType kernal, basic, chargen;
      params.FindValue(Parameters::ZXTune::Core::Plugins::SID::KERNAL, kernal);
      params.FindValue(Parameters::ZXTune::Core::Plugins::SID::BASIC, basic);
      params.FindValue(Parameters::ZXTune::Core::Plugins::SID::CHARGEN, chargen);
      Player.setRoms(GetData(kernal, GetKernalROM()), GetData(basic, GetBasicROM()), GetData(chargen, GetChargenROM()));
      const uint_t chipsCount = Player.info().maxsids();
      Builder.create(chipsCount);
      Config.frequency = samplerate;
    }

    void Load(SidTune& tune)
    {
      CheckSidplayError(Player.load(&tune));
    }

    void ApplyParameters(const SidParameters& sidParams)
    {
      const auto newFastSampling = sidParams.GetFastSampling();
      const auto newSamplingMethod = sidParams.GetSamplingMethod();
      const auto newFilter = sidParams.GetUseFilter();
      if (Config.fastSampling != newFastSampling || Config.samplingMethod != newSamplingMethod
          || UseFilter != newFilter)
      {
        Config.playback = Sound::Sample::CHANNELS == 1 ? SidConfig::MONO : SidConfig::STEREO;

        Config.fastSampling = newFastSampling;
        Config.samplingMethod = newSamplingMethod;
        Builder.filter(UseFilter = newFilter);

        Config.sidEmulation = &Builder;
        CheckSidplayError(Player.config(Config));
        SetClockRate(Player.getCPUFreq());
      }
    }

    void Stop()
    {
      Player.stop();
    }

    uint_t GetSoundFreq() const
    {
      return Config.frequency;
    }

    Sound::Chunk Render(uint_t samples)
    {
      static_assert(Sound::Sample::BITS == 16, "Incompatible sound bits count");
      Sound::Chunk result(samples);
      Player.play(safe_ptr_cast<short*>(result.data()), samples * Sound::Sample::CHANNELS);
      return result;
    }

    void Skip(uint_t samples)
    {
      Player.play(nullptr, samples * Sound::Sample::CHANNELS);
    }

    SpectrumState GetState() const override
    {
      unsigned freqs[6], levels[6];
      const auto count = Player.getState(freqs, levels);
      SpectrumState result;
      for (uint_t chan = 0; chan != count; ++chan)
      {
        const auto band = Analysis.GetBandByScaledFrequency(freqs[chan]);
        result.Set(band, LevelType(levels[chan], 15));
      }
      return result;
    }

  private:
    void SetClockRate(uint_t rate)
    {
      // Fout = (Fn * Fclk/16777216) Hz
      // http://www.waitingforfriday.com/index.php/Commodore_SID_6581_Datasheet
      Analysis.SetClockAndDivisor(rate, 16777216);
    }

  private:
    sidplayfp Player;
    ReSIDBuilder Builder;
    SidConfig Config;

    // cache filter flag
    bool UseFilter;
    Devices::Details::AnalysisMap Analysis;
  };

  const auto FRAME_DURATION = Time::Milliseconds(100);

  class Renderer : public Module::Renderer
  {
  public:
    Renderer(Model::Ptr tune, uint_t samplerate, Parameters::Accessor::Ptr params)
      : Tune(std::move(tune))
      , State(MakePtr<TimedState>(Tune->GetDuration()))
      , Engine(MakePtr<SidEngine>())
      , SidParams(MakePtr<SidParameters>(params))
    {
      Engine->Init(samplerate, *params);
      ApplyParameters();
      Engine->Load(*Tune);
    }

    Module::State::Ptr GetState() const override
    {
      return State;
    }

    Module::Analyzer::Ptr GetAnalyzer() const override
    {
      return Engine;
    }

    Sound::Chunk Render(const Sound::LoopParameters& looped) override
    {
      if (!State->IsValid())
      {
        return {};
      }
      const auto avail = State->Consume(FRAME_DURATION, looped);
      return Engine->Render(GetSamples(avail));
    }

    void Reset() override
    {
      SidParams.Reset();
      Engine->Stop();
      State->Reset();
    }

    void SetPosition(Time::AtMillisecond request) override
    {
      if (request < State->At())
      {
        Engine->Stop();
      }
      if (const auto toSkip = State->Seek(request))
      {
        Engine->Skip(GetSamples(toSkip));
      }
    }

  private:
    uint_t GetSamples(Time::Microseconds period) const
    {
      return period.Get() * Engine->GetSoundFreq() / period.PER_SECOND;
    }

    void ApplyParameters()
    {
      if (SidParams.IsChanged())
      {
        Engine->ApplyParameters(*SidParams);
      }
    }

  private:
    const Model::Ptr Tune;
    const TimedState::Ptr State;
    const SidEngine::Ptr Engine;
    const StateIterator::Ptr Iterator;
    Parameters::TrackingHelper<SidParameters> SidParams;
  };

  class Holder : public Module::Holder
  {
  public:
    Holder(Model::Ptr tune, Parameters::Accessor::Ptr props)
      : Tune(std::move(tune))
      , Properties(std::move(props))
    {}

    Module::Information::Ptr GetModuleInformation() const override
    {
      return CreateTimedInfo(Tune->GetDuration());
    }

    Parameters::Accessor::Ptr GetModuleProperties() const override
    {
      return Properties;
    }

    Renderer::Ptr CreateRenderer(uint_t samplerate, Parameters::Accessor::Ptr params) const override
    {
      return MakePtr<Renderer>(Tune, samplerate, std::move(params));
    }

  private:
    const Model::Ptr Tune;
    const Parameters::Accessor::Ptr Properties;
  };

  bool HasSidContainer(const Parameters::Accessor& params)
  {
    Parameters::StringType container;
    Require(params.FindValue(Module::ATTR_CONTAINER, container));
    return container == "SID" || boost::algorithm::ends_with(container, ">SID");
  }

  String DecodeString(StringView str)
  {
    return Strings::ToAutoUtf8(Strings::TrimSpaces(str));
  }

  class Factory : public Module::Factory
  {
  public:
    Module::Holder::Ptr CreateModule(const Parameters::Accessor& params, const Binary::Container& rawData,
                                     Parameters::Container::Ptr properties) const override
    {
      try
      {
        auto tune = MakePtr<Model>(rawData);

        const auto& tuneInfo = *tune->getInfo();
        if (tuneInfo.songs() > 1)
        {
          Require(HasSidContainer(*properties));
        }

        PropertiesHelper props(*properties);
        switch (tuneInfo.numberOfInfoStrings())
        {
        default:
        case 3:
          // copyright/publisher really
          props.SetComment(DecodeString(tuneInfo.infoString(2)));
          [[fallthrough]];
        case 2:
          props.SetAuthor(DecodeString(tuneInfo.infoString(1)));
          [[fallthrough]];
        case 1:
          props.SetTitle(DecodeString(tuneInfo.infoString(0)));
          [[fallthrough]];
        case 0:
          break;
        }
        auto data = rawData.GetSubcontainer(0, tuneInfo.dataFileLen());
        const auto size = data->Size();
        props.SetSource(*Formats::Chiptune::CreateCalculatingCrcContainer(std::move(data), 0, size));

        props.SetPlatform(Platforms::COMMODORE_64);

        tune->FillDuration(params);
        return MakePtr<Holder>(std::move(tune), std::move(properties));
      }
      catch (const std::exception&)
      {
        return {};
      }
    }
  };
}  // namespace Module::Sid

namespace ZXTune
{
  void RegisterSIDPlugins(PlayerPluginsRegistrator& registrator)
  {
    const Char ID[] = {'S', 'I', 'D', 0};
    const uint_t CAPS = Capabilities::Module::Type::MEMORYDUMP | Capabilities::Module::Device::MOS6581;
    const Formats::Chiptune::Decoder::Ptr decoder = Formats::Chiptune::CreateSIDDecoder();
    const Module::Factory::Ptr factory = MakePtr<Module::Sid::Factory>();
    const PlayerPlugin::Ptr plugin = CreatePlayerPlugin(ID, CAPS, decoder, factory);
    registrator.RegisterPlugin(plugin);
  }
}  // namespace ZXTune
