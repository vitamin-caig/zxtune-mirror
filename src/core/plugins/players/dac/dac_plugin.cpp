/**
* 
* @file
*
* @brief  DAC-based player plugin factory
*
* @author vitamin.caig@gmail.com
*
**/

//local includes
#include "core/plugins/players/dac/dac_plugin.h"
#include "core/plugins/players/plugin.h"
//common includes
#include <make_ptr.h>
//library includes
#include <core/plugin_attrs.h>
#include <module/players/dac/dac_base.h>
#include <module/players/dac/dac_parameters.h>
#include <sound/mixer_factory.h>
//std includes
#include <utility>

namespace Module
{
  template<unsigned Channels>
  Devices::DAC::Chip::Ptr CreateChip(uint_t samplerate, Parameters::Accessor::Ptr params)
  {
    typedef Sound::FixedChannelsMatrixMixer<Channels> MixerType;
    auto mixer = MixerType::Create();
    auto pollParams = Sound::CreateMixerNotificationParameters(std::move(params), mixer);
    auto chipParams = Module::DAC::CreateChipParameters(samplerate, std::move(pollParams));
    return Devices::DAC::CreateChip(std::move(chipParams), std::move(mixer));
  }

  Devices::DAC::Chip::Ptr CreateChip(unsigned channels, uint_t samplerate, Parameters::Accessor::Ptr params)
  {
    switch (channels)
    {
    case 3:
      return CreateChip<3>(samplerate, std::move(params));
    case 4:
      return CreateChip<4>(samplerate, std::move(params));
    default:
      return {};
    };
  }

  class DACHolder : public Holder
  {
  public:
    DACHolder(DAC::Chiptune::Ptr chiptune)
      : Tune(std::move(chiptune))
    {
    }

    Information::Ptr GetModuleInformation() const override
    {
      return CreateTrackInfo(Tune->GetFrameDuration(), Tune->GetTrackModel());
    }

    Parameters::Accessor::Ptr GetModuleProperties() const override
    {
      return Tune->GetProperties();
    }

    Renderer::Ptr CreateRenderer(uint_t samplerate, Parameters::Accessor::Ptr params, Sound::Receiver::Ptr target) const override
    {
      auto iterator = Tune->CreateDataIterator();
      auto chip = CreateChip(Tune->GetTrackModel()->GetChannelsCount(), samplerate, std::move(params));
      Tune->GetSamples(*chip);
      return DAC::CreateRenderer(Tune->GetFrameDuration()/*TODO: speed variation*/, std::move(iterator), std::move(chip), std::move(target));
    }
  private:
    const DAC::Chiptune::Ptr Tune;
  };

  class DACFactory : public Factory
  {
  public:
    explicit DACFactory(DAC::Factory::Ptr delegate)
      : Delegate(std::move(delegate))
    {
    }

    Holder::Ptr CreateModule(const Parameters::Accessor& params, const Binary::Container& data, Parameters::Container::Ptr properties) const override
    {
      if (auto chiptune = Delegate->CreateChiptune(data, std::move(properties)))
      {
        return MakePtr<DACHolder>(std::move(chiptune));
      }
      else
      {
        return {};
      }
    }
  private:
    const DAC::Factory::Ptr Delegate;
  };
}

namespace ZXTune
{
  PlayerPlugin::Ptr CreatePlayerPlugin(const String& id, Formats::Chiptune::Decoder::Ptr decoder, Module::DAC::Factory::Ptr factory)
  {
    const Module::Factory::Ptr modFactory = MakePtr<Module::DACFactory>(factory);
    const uint_t caps = Capabilities::Module::Type::TRACK | Capabilities::Module::Device::DAC;
    return CreatePlayerPlugin(id, caps, decoder, modFactory);
  }
}
