/**
*
* @file
*
* @brief  2SF chiptune factory implementation
*
* @author vitamin.caig@gmail.com
*
**/

//local includes
#include "module/players/xsf/2sf.h"
#include "module/players/xsf/memory_region.h"
#include "module/players/xsf/xsf.h"
#include "module/players/xsf/xsf_factory.h"
//common includes
#include <contract.h>
#include <make_ptr.h>
//library includes
#include <binary/compression/zlib_container.h>
#include <debug/log.h>
#include <devices/details/analysis_map.h>
#include <formats/chiptune/emulation/nintendodssoundformat.h>
#include <math/bitops.h>
#include <module/attributes.h>
#include <module/players/analyzer.h>
#include <module/players/streaming.h>
#include <sound/resampler.h>
//std includes
#include <list>
//3rdparty includes
#include <3rdparty/vio2sf/desmume/SPU.h>
#include <3rdparty/vio2sf/desmume/state.h>
//text includes
#include <module/text/platforms.h>

namespace Module
{
namespace TwoSF
{
  const Debug::Stream Dbg("Module::2SF");
  
  struct ModuleData
  {
    using Ptr = std::shared_ptr<const ModuleData>;
    using RWPtr = std::shared_ptr<ModuleData>;
    
    ModuleData() = default;
    ModuleData(const ModuleData&) = delete;
    
    std::list<Binary::Container::Ptr> PackedProgramSections;
    std::list<Binary::Container::Ptr> ReservedSections;

    XSF::MetaInformation::Ptr Meta;

    uint_t GetRefreshRate() const
    {
      return Meta->RefreshRate ? Meta->RefreshRate : 50;
    }
  };
  
  class AnalysisMap
  {
  public:
    AnalysisMap()
    {
      const auto CLOCKRATE = 33513982 / 2;
      const auto C3_RATE = 130.81f;
      const auto C3_SAMPLERATE = 8000;
      Delegate.SetClockRate(CLOCKRATE * C3_RATE / C3_SAMPLERATE);
    }
    
    uint_t GetBand(uint_t timer) const
    {
      return Delegate.GetBandByPeriod(0x10000 - timer);
    }
  private:
    Devices::Details::AnalysisMap Delegate;
  };
  
  class DSEngine : public Module::Analyzer
  {
  public:
    using Ptr = std::shared_ptr<DSEngine>;
  
    enum
    {
      SAMPLERATE = 44100
    };
    
    explicit DSEngine(const ModuleData& data)
    {
      Require(0 == ::state_init(&State));
      if (data.Meta)
      {
        SetupEnvironment(*data.Meta);
      }
      if (!data.PackedProgramSections.empty())
      {
        SetupRom(data.PackedProgramSections);
      }
      if (!data.ReservedSections.empty())
      {
        SetupState(data.ReservedSections);
      }
    }
    
    ~DSEngine() override
    {
      ::state_deinit(&State);
    }
    
    Sound::Chunk Render(uint_t samples)
    {
      static_assert(Sound::Sample::CHANNELS == 2, "Invalid sound channels count");
      static_assert(Sound::Sample::MID == 0, "Invalid sound sample type");
      static_assert(Sound::Sample::MAX == 32767 && Sound::Sample::MIN == -32768, "Invalid sound sample type");

      Sound::Chunk res(samples);
      ::state_render(&State, safe_ptr_cast<s16*>(res.data()), samples);
      return res;
    }
    
    void Skip(uint_t samples)
    {
      ::state_render(&State, nullptr, samples);
    }

    SpectrumState GetState() const override
    {
      static const AnalysisMap ANALYSIS;
      SpectrumState result;
      for (const auto& in : State.SPU_core->channels)
      {
        if (in.status == CHANSTAT_STOPPED)
        {
          continue;
        }
        if (const auto level = ::spumuldiv7(100, in.vol) >> in.datashift)
        {
          const auto band = ANALYSIS.GetBand(in.timer);
          result.Set(band, LevelType(level, 100));
        }
      }
      return result;
    }
  private:
    void SetupEnvironment(const XSF::MetaInformation& meta)
    {
      int clockDown = 0;
      State.dwChannelMute = 0;
      State.initial_frames = -1;
      //TODO: interpolation
      for (const auto& tag : meta.Tags)
      {
        if (tag.first == "_clockdown")
        {
          clockDown = std::atoi(tag.second.c_str());
        }
        else if (auto target = FindTagTarget(tag.first))
        {
          *target = std::atoi(tag.second.c_str());
        }
      }
      if (!State.arm7_clockdown_level && clockDown)
      {
        State.arm7_clockdown_level = clockDown;
      }
      if (!State.arm9_clockdown_level && clockDown)
      {
        State.arm9_clockdown_level = clockDown;
      }
    }
    
    int* FindTagTarget(const String& name)
    {
      if (name == "_frames")
      {
        return &State.initial_frames;
      }
      else if (name == "_vio2sf_sync_type")
      {
        return &State.sync_type;
      }
      else if (name == "_vio2sf_arm9_clockdown_level")
      {
        return &State.arm9_clockdown_level;
      }
      else if (name == "_vio2sf_arm7_clockdown_level")
      {
        return &State.arm7_clockdown_level;
      }
      else
      {
        return nullptr;
      }
    }
    
    void SetupRom(const std::list<Binary::Container::Ptr>& blocks)
    {
      ChunkBuilder builder;
      for (const auto& block : blocks)
      {
        const auto unpacked = Binary::Compression::Zlib::Decompress(*block);
        Formats::Chiptune::NintendoDSSoundFormat::ParseRom(*unpacked, builder);
      }
      //possibly, emulation writes to ROM are, so copy it
      Rom = builder.CaptureResult();
      //required power of 2 size
      const auto alignedRomSize = uint32_t(1) << Math::Log2(Rom.Data.size());
      Rom.Data.resize(alignedRomSize);
      ::state_setrom(&State, Rom.Data.data(), alignedRomSize);
    }
    
    void SetupState(const std::list<Binary::Container::Ptr>& blocks)
    {
      ChunkBuilder builder;
      for (const auto& block : blocks)
      {
        Formats::Chiptune::NintendoDSSoundFormat::ParseState(*block, builder);
      }
      const auto& state = builder.CaptureResult();
      ::state_loadstate(&State, state.Data.data(), state.Data.size());
    }
  private:
    class ChunkBuilder : public Formats::Chiptune::NintendoDSSoundFormat::Builder
    {
    public:
      void SetChunk(uint32_t offset, Binary::View content) override
      {
        Result.Update(offset, content);
      }
      
      MemoryRegion CaptureResult()
      {
        return std::move(Result);
      }
    private:
      MemoryRegion Result;
    };
  private:
    NDS_state State;
    MemoryRegion Rom;
  };

  const auto FRAME_DURATION = Time::Milliseconds(100);

  uint_t GetSamples(Time::Microseconds period)
  {
    return period.Get() * DSEngine::SAMPLERATE / period.PER_SECOND;
  }
  
  class Renderer : public Module::Renderer
  {
  public:
    Renderer(ModuleData::Ptr data, Sound::Receiver::Ptr target)
      : Data(std::move(data))
      , State(MakePtr<TimedState>(Data->Meta->Duration))
      , Target(std::move(target))
      , Engine(MakePtr<DSEngine>(*Data))
    {
    }

    Module::State::Ptr GetState() const override
    {
      return State;
    }

    Module::Analyzer::Ptr GetAnalyzer() const override
    {
      return Engine;
    }

    bool RenderFrame(const Sound::LoopParameters& looped) override
    {
      const auto avail = State->Consume(FRAME_DURATION, looped);
      Target->ApplyData(Engine->Render(GetSamples(avail)));
      return State->IsValid();
    }

    void Reset() override
    {
      State->Reset();
      Engine = MakePtr<DSEngine>(*Data);
    }

    void SetPosition(Time::AtMillisecond request) override
    {
      if (request < State->At())
      {
        Engine = MakePtr<DSEngine>(*Data);
      }
      const auto toSkip = State->Seek(request);
      if (toSkip.Get())
      {
        Engine->Skip(GetSamples(toSkip));
      }
    }
  private:
    const ModuleData::Ptr Data;
    const TimedState::Ptr State;
    const Sound::Receiver::Ptr Target;
    DSEngine::Ptr Engine;
  };

  class Holder : public Module::Holder
  {
  public:
    Holder(ModuleData::Ptr tune, Parameters::Accessor::Ptr props)
      : Tune(std::move(tune))
      , Properties(std::move(props))
    {
    }

    Module::Information::Ptr GetModuleInformation() const override
    {
      return CreateTimedInfo(Tune->Meta->Duration);
    }

    Parameters::Accessor::Ptr GetModuleProperties() const override
    {
      return Properties;
    }

    Renderer::Ptr CreateRenderer(uint_t samplerate, Parameters::Accessor::Ptr /*params*/, Sound::Receiver::Ptr target) const override
    {
      return MakePtr<Renderer>(Tune, Sound::CreateResampler(DSEngine::SAMPLERATE, samplerate, std::move(target)));
    }
    
    static Ptr Create(ModuleData::Ptr tune, Parameters::Container::Ptr properties)
    {
      if (tune->Meta)
      {
        tune->Meta->Dump(*properties);
      }
      properties->SetValue(ATTR_PLATFORM, Platforms::NINTENDO_DS);
      return MakePtr<Holder>(std::move(tune), std::move(properties));
    }
  private:
    const ModuleData::Ptr Tune;
    const Parameters::Accessor::Ptr Properties;
  };

  class ModuleDataBuilder
  {
  public:
    ModuleDataBuilder()
      : Result(MakeRWPtr<ModuleData>())
    {
    }
    
    void AddProgramSection(Binary::Container::Ptr packedSection)
    {
      Require(!!packedSection);
      Result->PackedProgramSections.push_back(std::move(packedSection));
    }
    
    void AddReservedSection(Binary::Container::Ptr reservedSection)
    {
      Require(!!reservedSection);
      Result->ReservedSections.push_back(std::move(reservedSection));
    }
    
    void AddMeta(const XSF::MetaInformation& meta)
    {
      if (!Meta)
      {
        Result->Meta = Meta = MakeRWPtr<XSF::MetaInformation>(meta);
      }
      else
      {
        Meta->Merge(meta);
      }
    }
    
    ModuleData::Ptr CaptureResult()
    {
      return Result;
    }
  private:
    const ModuleData::RWPtr Result;
    XSF::MetaInformation::RWPtr Meta;
  };
  
  class Factory : public XSF::Factory
  {
  public:
    Holder::Ptr CreateSinglefileModule(const XSF::File& file, Parameters::Container::Ptr properties) const override
    {
      ModuleDataBuilder builder;
      if (file.PackedProgramSection)
      {
        builder.AddProgramSection(file.PackedProgramSection);
      }
      if (file.ReservedSection)
      {
        builder.AddReservedSection(file.ReservedSection);
      }
      if (file.Meta)
      {
        builder.AddMeta(*file.Meta);
      }
      return Holder::Create(builder.CaptureResult(), std::move(properties));
    }
    
    Holder::Ptr CreateMultifileModule(const XSF::File& file, const std::map<String, XSF::File>& additionalFiles, Parameters::Container::Ptr properties) const override
    {
      ModuleDataBuilder builder;
      MergeSections(file, additionalFiles, builder);
      MergeMeta(file, additionalFiles, builder);
      return Holder::Create(builder.CaptureResult(), std::move(properties));
    }
  private:
    static const uint_t MAX_LEVEL = 10;

    static void MergeSections(const XSF::File& data, const std::map<String, XSF::File>& additionalFiles, ModuleDataBuilder& dst, uint_t level = 1)
    {
      if (!data.Dependencies.empty() && level < MAX_LEVEL)
      {
        MergeSections(additionalFiles.at(data.Dependencies.front()), additionalFiles, dst, level + 1);
      }
      if (data.PackedProgramSection)
      {
        dst.AddProgramSection(data.PackedProgramSection);
      }
      if (data.ReservedSection)
      {
        dst.AddReservedSection(data.ReservedSection);
      }
    }
    
    static void MergeMeta(const XSF::File& data, const std::map<String, XSF::File>& additionalFiles, ModuleDataBuilder& dst, uint_t level = 1)
    {
      if (level < MAX_LEVEL)
      {
        for (const auto& dep : data.Dependencies)
        {
          MergeMeta(additionalFiles.at(dep), additionalFiles, dst, level + 1);
        }
      }
      if (data.Meta)
      {
        dst.AddMeta(*data.Meta);
      }
    }
  };
  
  Module::Factory::Ptr CreateFactory()
  {
    return XSF::CreateFactory(MakePtr<Factory>());
  }
}
}
