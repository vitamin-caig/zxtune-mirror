/**
 *
 * @file
 *
 * @brief  TurboSound chip implementation
 *
 * @author vitamin.caig@gmail.com
 *
 **/

// local includes
#include "devices/aym/src/psg.h"
#include "devices/aym/src/soundchip.h"
// common includes
#include <make_ptr.h>
// library includes
#include <devices/turbosound.h>
// std includes
#include <utility>

namespace Devices::TurboSound
{
  class PSG
  {
  public:
    explicit PSG(const AYM::MultiVolumeTable& table)
      : Chip0(table)
      , Chip1(table)
    {}

    void SetDutyCycle(uint_t value, uint_t mask)
    {
      Chip0.SetDutyCycle(value, mask);
      Chip1.SetDutyCycle(value, mask);
    }

    void Reset()
    {
      Chip0.Reset();
      Chip1.Reset();
    }

    void SetNewData(const Registers& data)
    {
      Chip0.SetNewData(data[0]);
      Chip1.SetNewData(data[1]);
    }

    void Tick(uint_t ticks)
    {
      Chip0.Tick(ticks);
      Chip1.Tick(ticks);
    }

    Sound::Sample GetLevels() const
    {
      using namespace Sound;
      const Sample s0 = Chip0.GetLevels();
      const Sample s1 = Chip1.GetLevels();
      return Sound::Sample::FastAdd(s0, s1);
    }

    void GetState(const Details::AnalysisMap& analyzer, DeviceState& state) const
    {
      Chip0.GetState(analyzer, state);
      Chip1.GetState(analyzer, state);
    }

  private:
    AYM::PSG Chip0;
    AYM::PSG Chip1;
  };

  struct Traits
  {
    typedef DataChunk DataChunkType;
    typedef PSG PSGType;
    typedef Chip ChipBaseType;
    static const uint_t VOICES = TurboSound::VOICES;
  };

  class HalfLevelMixer : public MixerType
  {
  public:
    explicit HalfLevelMixer(MixerType::Ptr delegate)
      : Delegate(std::move(delegate))
      , DelegateRef(*Delegate)
    {}

    Sound::Sample ApplyData(const MixerType::InDataType& in) const override
    {
      const Sound::Sample out = DelegateRef.ApplyData(in);
      return Sound::Sample(out.Left() / 2, out.Right() / 2);
    }

  private:
    const MixerType::Ptr Delegate;
    const MixerType& DelegateRef;
  };

  Chip::Ptr CreateChip(ChipParameters::Ptr params, MixerType::Ptr mixer)
  {
    auto halfMixer = MakePtr<HalfLevelMixer>(mixer);
    return MakePtr<AYM::SoundChip<Traits> >(std::move(params), std::move(halfMixer));
  }
}  // namespace Devices::TurboSound
