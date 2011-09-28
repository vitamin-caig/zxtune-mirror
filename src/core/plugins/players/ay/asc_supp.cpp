/*
Abstract:
  ASC modules playback support

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "ay_base.h"
#include "ay_conversion.h"
#include "core/plugins/utils.h"
#include "core/plugins/registrator.h"
#include "core/plugins/players/creation_result.h"
#include "core/plugins/players/module_properties.h"
//common includes
#include <byteorder.h>
#include <error_tools.h>
#include <logging.h>
#include <range_checker.h>
#include <tools.h>
//library includes
#include <core/convert_parameters.h>
#include <core/core_parameters.h>
#include <core/error_codes.h>
#include <core/module_attrs.h>
#include <core/plugin_attrs.h>
#include <io/container.h>
//text includes
#include <core/text/core.h>
#include <core/text/plugins.h>
#include <core/text/warnings.h>

#define FILE_TAG 45B26E38

namespace
{
  using namespace ZXTune;
  using namespace ZXTune::Module;

  const uint_t LIMITER(~uint_t(0));

  //hints
  const std::size_t MAX_MODULE_SIZE = 0x3800;
  const uint_t SAMPLES_COUNT = 32;
  const uint_t MAX_SAMPLE_SIZE = 150;
  const uint_t ORNAMENTS_COUNT = 32;
  const uint_t MAX_ORNAMENT_SIZE = 30;
  const uint_t MAX_PATTERN_SIZE = 64;//???
  const uint_t MAX_PATTERNS_COUNT = 32;//TODO

  //////////////////////////////////////////////////////////////////////////
#ifdef USE_PRAGMA_PACK
#pragma pack(push,1)
#endif
  PACK_PRE struct ASCHeader
  {
    uint8_t Tempo;
    uint8_t Loop;
    uint16_t PatternsOffset;
    uint16_t SamplesOffset;
    uint16_t OrnamentsOffset;
    uint8_t Length;
    uint8_t Positions[1];
  } PACK_POST;

  const uint8_t ASC_ID_1[] =
  {
    'A', 'S', 'M', ' ', 'C', 'O', 'M', 'P', 'I', 'L', 'A', 'T', 'I', 'O', 'N', ' ', 'O', 'F', ' '
  };
  const uint8_t ASC_ID_2[] =
  {
    ' ', 'B', 'Y', ' '
  };

  PACK_PRE struct ASCID
  {
    uint8_t Identifier1[19];//'ASM COMPILATION OF '
    char Title[20];
    uint8_t Identifier2[4];//' BY '
    char Author[20];

    bool Check() const
    {
      BOOST_STATIC_ASSERT(sizeof(ASC_ID_1) == sizeof(Identifier1));
      BOOST_STATIC_ASSERT(sizeof(ASC_ID_2) == sizeof(Identifier2));
      return 0 == std::memcmp(Identifier1, ASC_ID_1, sizeof(Identifier1)) &&
             0 == std::memcmp(Identifier2, ASC_ID_2, sizeof(Identifier2));
    }
  } PACK_POST;

  PACK_PRE struct ASCPattern
  {
    boost::array<uint16_t, 3> Offsets;//from start of patterns
  } PACK_POST;

  PACK_PRE struct ASCOrnaments
  {
    boost::array<uint16_t, ORNAMENTS_COUNT> Offsets;
  } PACK_POST;

  PACK_PRE struct ASCOrnament
  {
    PACK_PRE struct Line
    {
      //BEFooooo
      //OOOOOOOO

      //o - noise offset (signed)
      //B - loop begin
      //E - loop end
      //F - finished
      //O - note offset (signed)
      uint8_t LoopAndNoiseOffset;
      int8_t NoteOffset;

      bool IsLoopBegin() const
      {
        return 0 != (LoopAndNoiseOffset & 128);
      }

      bool IsLoopEnd() const
      {
        return 0 != (LoopAndNoiseOffset & 64);
      }

      bool IsFinished() const
      {
        return 0 != (LoopAndNoiseOffset & 32);
      }

      int_t GetNoiseOffset() const
      {
        return static_cast<int8_t>(LoopAndNoiseOffset * 8) / 8;
      }
    } PACK_POST;
    Line Data[1];
  } PACK_POST;

  PACK_PRE struct ASCSamples
  {
    boost::array<uint16_t, SAMPLES_COUNT> Offsets;
  } PACK_POST;

  PACK_PRE struct ASCSample
  {
    PACK_PRE struct Line
    {
      //BEFaaaaa
      //TTTTTTTT
      //LLLLnCCt
      //a - adding
      //B - loop begin
      //E - loop end
      //F - finished
      //T - tone deviation
      //L - level
      //n - noise mask
      //C - command
      //t - tone mask

      uint8_t LoopAndAdding;
      int8_t ToneDeviation;
      uint8_t LevelAndMasks;

      bool IsLoopBegin() const
      {
        return 0 != (LoopAndAdding & 128);
      }

      bool IsLoopEnd() const
      {
        return 0 != (LoopAndAdding & 64);
      }

      bool IsFinished() const
      {
        return 0 != (LoopAndAdding & 32);
      }

      int_t GetAdding() const
      {
        return static_cast<int8_t>(LoopAndAdding * 8) / 8;
      }

      uint_t GetLevel() const
      {
        return LevelAndMasks >> 4;
      }

      bool GetNoiseMask() const
      {
        return 0 != (LevelAndMasks & 8);
      }

      uint_t GetCommand() const
      {
        return (LevelAndMasks & 6) >> 1;
      }

      bool GetToneMask() const
      {
        return 0 != (LevelAndMasks & 1);
      }
    } PACK_POST;

    enum
    {
      EMPTY,
      ENVELOPE,
      DECVOLADD,
      INCVOLADD
    };
    Line Data[1];
  } PACK_POST;

#ifdef USE_PRAGMA_PACK
#pragma pack(pop)
#endif

  BOOST_STATIC_ASSERT(sizeof(ASCHeader) == 10);
  BOOST_STATIC_ASSERT(sizeof(ASCPattern) == 6);
  BOOST_STATIC_ASSERT(sizeof(ASCOrnament) == 2);
  BOOST_STATIC_ASSERT(sizeof(ASCSample) == 3);

  struct Sample
  {
    explicit Sample(const ASCSample& sample)
      : Loop(), LoopLimit(), Lines()
    {
      Lines.reserve(MAX_SAMPLE_SIZE);
      for (uint_t sline = 0; sline != MAX_SAMPLE_SIZE; ++sline)
      {
        const ASCSample::Line& line = sample.Data[sline];
        Lines.push_back(Line(line));
        if (line.IsLoopBegin())
        {
          Loop = sline;
        }
        if (line.IsLoopEnd())
        {
          LoopLimit = sline;
        }
        if (line.IsFinished())
        {
          break;
        }
      }
    }

    struct Line
    {
      Line()
        : Level(), ToneDeviation(), ToneMask(), NoiseMask(), Adding(), Command()
      {
      }

      explicit Line(const ASCSample::Line& line)
        : Level(line.GetLevel()), ToneDeviation(line.ToneDeviation)
        , ToneMask(line.GetToneMask()), NoiseMask(line.GetNoiseMask())
        , Adding(line.GetAdding()), Command(line.GetCommand())
      {
      }
      uint_t Level;//0-15
      int_t ToneDeviation;
      bool ToneMask;
      bool NoiseMask;
      int_t Adding;
      uint_t Command;
    };

    uint_t GetLoop() const
    {
      return Loop;
    }

    uint_t GetLoopLimit() const
    {
      return LoopLimit;
    }

    uint_t GetSize() const
    {
      return static_cast<uint_t>(Lines.size());
    }

    const Line& GetLine(uint_t idx) const
    {
      static const Line STUB;
      return Lines.size() > idx ? Lines[idx] : STUB;
    }
  private:
    uint_t Loop;
    uint_t LoopLimit;
    std::vector<Line> Lines;
  };

  struct Ornament
  {
    explicit Ornament(const ASCOrnament& ornament)
      : Loop(), LoopLimit(), Lines()
    {
      Lines.reserve(MAX_ORNAMENT_SIZE);
      for (uint_t sline = 0; sline != MAX_ORNAMENT_SIZE; ++sline)
      {
        const ASCOrnament::Line& line = ornament.Data[sline];
        Lines.push_back(Line(line));
        if (line.IsLoopBegin())
        {
          Loop = sline;
        }
        if (line.IsLoopEnd())
        {
          LoopLimit = sline;
        }
        if (line.IsFinished())
        {
          break;
        }
      }
    }

    struct Line
    {
      Line()
        : NoteAddon(), NoiseAddon()
      {
      }

      explicit Line(const ASCOrnament::Line& line)
        : NoteAddon(line.NoteOffset)
        , NoiseAddon(line.GetNoiseOffset())
      {
      }
      int_t NoteAddon;
      int_t NoiseAddon;
    };

    uint_t GetLoop() const
    {
      return Loop;
    }

    uint_t GetLoopLimit() const
    {
      return LoopLimit;
    }

    uint_t GetSize() const
    {
      return static_cast<uint_t>(Lines.size());
    }

    const Line& GetLine(uint_t idx) const
    {
      static const Line STUB;
      return Lines.size() > idx ? Lines[idx] : STUB;
    }
  private:
    uint_t Loop;
    uint_t LoopLimit;
    std::vector<Line> Lines;
  };

  //supported commands and their parameters
  enum CmdType
  {
    //no parameters
    EMPTY,
    //r13,envL
    ENVELOPE,
    //no parameters
    ENVELOPE_ON,
    //no parameters
    ENVELOPE_OFF,
    //value
    NOISE,
    //no parameters
    CONT_SAMPLE,
    //no parameters
    CONT_ORNAMENT,
    //glissade value
    GLISS,
    //steps
    SLIDE,
    //steps,note
    SLIDE_NOTE,
    //period,delta
    AMPLITUDE_SLIDE,
    //no parameter
    BREAK_SAMPLE
  };

  // tracker type
  typedef TrackingSupport<Devices::AYM::CHANNELS, CmdType, Sample, Ornament> ASCTrack;

  class ASCModuleData : public ASCTrack::ModuleData
  {
  public:
    typedef boost::shared_ptr<ASCModuleData> Ptr;

    ASCModuleData()
      : ASCTrack::ModuleData()
    {
    }

    void ParseInformation(const IO::FastDump& data, ModuleProperties& properties)
    {
      const ASCHeader* const header = safe_ptr_cast<const ASCHeader*>(&data[0]);

      LoopPosition = header->Loop;
      InitialTempo = header->Tempo;
      //meta properties
      const ASCID* const id = safe_ptr_cast<const ASCID*>(header->Positions + header->Length);
      if (id->Check())
      {
        properties.SetTitle(OptimizeString(FromCharArray(id->Title)));
        properties.SetAuthor(OptimizeString(FromCharArray(id->Author)));
      }
      properties.SetProgram(Text::ASC_EDITOR);
      properties.SetFreqtable(TABLE_ASM);
    }

    std::size_t ParseSamples(const IO::FastDump& data)
    {
      const ASCHeader* const header = safe_ptr_cast<const ASCHeader*>(&data[0]);

      std::size_t res = sizeof(*header);
      const std::size_t samplesOff = fromLE(header->SamplesOffset);
      const ASCSamples* const samples = safe_ptr_cast<const ASCSamples*>(&data[samplesOff]);
      Samples.reserve(samples->Offsets.size());
      uint_t index = 0;
      for (const uint16_t* pSample = samples->Offsets.begin(); pSample != samples->Offsets.end();
        ++pSample, ++index)
      {
        assert(*pSample && fromLE(*pSample) < data.Size());
        const std::size_t sampleOffset = fromLE(*pSample);
        const ASCSample* const sample = safe_ptr_cast<const ASCSample*>(&data[samplesOff + sampleOffset]);
        Samples.push_back(Sample(*sample));
        const Sample& smp = Samples.back();
        res = std::max<std::size_t>(res, samplesOff + sampleOffset + smp.GetSize() * sizeof(ASCSample::Line));
      }
      return res;
    }

    std::size_t ParseOrnaments(const IO::FastDump& data)
    {
      const ASCHeader* const header = safe_ptr_cast<const ASCHeader*>(&data[0]);

      std::size_t res = sizeof(*header);
      const std::size_t ornamentsOff = fromLE(header->OrnamentsOffset);
      const ASCOrnaments* const ornaments = safe_ptr_cast<const ASCOrnaments*>(&data[ornamentsOff]);
      Ornaments.reserve(ornaments->Offsets.size());
      uint_t index = 0;
      for (const uint16_t* pOrnament = ornaments->Offsets.begin(); pOrnament != ornaments->Offsets.end();
        ++pOrnament, ++index)
      {
        assert(*pOrnament && fromLE(*pOrnament) < data.Size());
        const std::size_t ornamentOffset = fromLE(*pOrnament);
        const ASCOrnament* const ornament = safe_ptr_cast<const ASCOrnament*>(&data[ornamentsOff + ornamentOffset]);
        Ornaments.push_back(ASCTrack::Ornament(*ornament));
        const Ornament& orn = Ornaments.back();
        res = std::max<std::size_t>(res, ornamentsOff + ornamentOffset + orn.GetSize() * sizeof(ASCOrnament::Line));
      }
      return res;
    }

    std::size_t ParsePatterns(const IO::FastDump& data)
    {
      const ASCHeader* const header = safe_ptr_cast<const ASCHeader*>(&data[0]);

      std::size_t res = sizeof(*header);
      //fill order
      Positions.assign(header->Positions, header->Positions + header->Length);
      //parse patterns
      const std::size_t patternsCount = 1 + *std::max_element(Positions.begin(), Positions.end());
      const uint16_t patternsOff = fromLE(header->PatternsOffset);
      const ASCPattern* pattern = safe_ptr_cast<const ASCPattern*>(&data[patternsOff]);
      assert(patternsCount <= MAX_PATTERNS_COUNT);
      Patterns.resize(patternsCount);
      for (uint_t patNum = 0; patNum < patternsCount; ++patNum, ++pattern)
      {
        ASCTrack::Pattern& pat(Patterns[patNum]);

        AYM::PatternCursors cursors;
        std::transform(pattern->Offsets.begin(), pattern->Offsets.end(), cursors.begin(),
          boost::bind(std::plus<uint_t>(), patternsOff, boost::bind(&fromLE<uint16_t>, _1)));
        uint_t envelopes = 0;
        uint_t& channelACursor = cursors.front().Offset;
        do
        {
          ASCTrack::Line& line = pat.AddLine();
          ParsePattern(data, cursors, line, envelopes);
          //skip lines
          if (const uint_t linesToSkip = cursors.GetMinCounter())
          {
            cursors.SkipLines(linesToSkip);
            pat.AddLines(linesToSkip);
          }
        }
        while (channelACursor < data.Size() &&
          (0xff != data[channelACursor] || 0 != cursors.front().Counter));
        res = std::max<std::size_t>(res, 1 + cursors.GetMaxOffset());
      }
      return res;
    }
  private:
    static void ParsePattern(const IO::FastDump& data
      , AYM::PatternCursors& cursors
      , ASCTrack::Line& line
      , uint_t& envelopes
      )
    {
      assert(line.Channels.size() == cursors.size());
      ASCTrack::Line::ChannelsArray::iterator channel = line.Channels.begin();
      uint_t envMask = 1;
      for (AYM::PatternCursors::iterator cur = cursors.begin(); cur != cursors.end(); ++cur, ++channel, envMask <<= 1)
      {
        if (cur->Counter--)
        {
          continue;//has to skip
        }

        bool continueSample = false;
        for (const std::size_t dataSize = data.Size(); cur->Offset < dataSize;)
        {
          const uint_t cmd = data[cur->Offset++];
          const std::size_t restbytes = dataSize - cur->Offset;
          if (cmd <= 0x55)//note
          {
            if (!continueSample)
            {
              channel->SetEnabled(true);
            }
            if (!channel->Commands.empty() &&
                SLIDE == channel->Commands.back().Type)
            {
              //set slide to note
              ASCTrack::Command& command = channel->Commands.back();
              command.Type = SLIDE_NOTE;
              command.Param2 = cmd;
            }
            else
            {
              channel->SetNote(cmd);
            }
            if (envelopes & envMask)
            {
              //modify existing
              ASCTrack::CommandsArray::iterator cmdIt = std::find(channel->Commands.begin(),
                channel->Commands.end(), ENVELOPE);
              if (restbytes < 1)
              {
                throw Error(THIS_LINE, ERROR_INVALID_FORMAT);//no details
              }
              const uint_t param = data[cur->Offset++];
              if (channel->Commands.end() == cmdIt)
              {
                channel->Commands.push_back(ASCTrack::Command(ENVELOPE, -1, param));
              }
              else
              {
                cmdIt->Param2 = param;
              }
            }
            break;
          }
          else if (cmd >= 0x56 && cmd <= 0x5d) //stop
          {
            break;
          }
          else if (cmd == 0x5e) //break sample loop
          {
            channel->Commands.push_back(ASCTrack::Command(BREAK_SAMPLE));
            break;
          }
          else if (cmd == 0x5f) //shut
          {
            channel->SetEnabled(false);
            break;
          }
          else if (cmd >= 0x60 && cmd <= 0x9f) //skip
          {
            cur->Period = cmd - 0x60;
          }
          else if (cmd >= 0xa0 && cmd <= 0xbf) //sample
          {
            channel->SetSample(cmd - 0xa0);
          }
          else if (cmd >= 0xc0 && cmd <= 0xdf) //ornament
          {
            channel->SetOrnament(cmd - 0xc0);
          }
          else if (cmd == 0xe0) // envelope full vol
          {
            channel->SetVolume(15);
            channel->Commands.push_back(ASCTrack::Command(ENVELOPE_ON));
            envelopes |= envMask;
          }
          else if (cmd >= 0xe1 && cmd <= 0xef) // noenvelope vol
          {
            channel->SetVolume(cmd - 0xe0);
            channel->Commands.push_back(ASCTrack::Command(ENVELOPE_OFF));
            envelopes &= ~envMask;
          }
          else if (cmd == 0xf0) //initial noise
          {
            if (restbytes < 1)
            {
              throw Error(THIS_LINE, ERROR_INVALID_FORMAT);//no details
            }
            channel->Commands.push_back(ASCTrack::Command(NOISE, data[cur->Offset++]));
          }
          else if ((cmd & 0xfc) == 0xf0) //0xf1, 0xf2, 0xf3- continue sample or ornament
          {
            if (cmd & 1)
            {
              continueSample = true;
              channel->Commands.push_back(ASCTrack::Command(CONT_SAMPLE));
            }
            if (cmd & 2)
            {
              channel->Commands.push_back(ASCTrack::Command(CONT_ORNAMENT));
            }
          }
          else if (cmd == 0xf4) //tempo
          {
            if (restbytes < 1)
            {
              throw Error(THIS_LINE, ERROR_INVALID_FORMAT);//no details
            }
            line.SetTempo(data[cur->Offset++]);
          }
          else if (cmd == 0xf5 || cmd == 0xf6) //slide
          {
            if (restbytes < 1)
            {
              throw Error(THIS_LINE, ERROR_INVALID_FORMAT);//no details
            }
            channel->Commands.push_back(ASCTrack::Command(GLISS,
              ((cmd == 0xf5) ? -16 : 16) * static_cast<int8_t>(data[cur->Offset++])));
          }
          else if (cmd == 0xf7 || cmd == 0xf9) //stepped slide
          {
            if (cmd == 0xf7)
            {
              channel->Commands.push_back(ASCTrack::Command(CONT_SAMPLE));
            }
            if (restbytes < 1)
            {
              throw Error(THIS_LINE, ERROR_INVALID_FORMAT);//no details
            }
            channel->Commands.push_back(ASCTrack::Command(SLIDE, static_cast<int8_t>(data[cur->Offset++])));
          }
          else if ((cmd & 0xf9) == 0xf8) //0xf8, 0xfa, 0xfc, 0xfe- envelope
          {
            ASCTrack::CommandsArray::iterator cmdIt = std::find(channel->Commands.begin(), channel->Commands.end(),
              ENVELOPE);
            if (channel->Commands.end() == cmdIt)
            {
              channel->Commands.push_back(ASCTrack::Command(ENVELOPE, cmd & 0xf, -1));
            }
            else
            {
              //strange situation...
              cmdIt->Param1 = cmd & 0xf;
            }
          }
          else if (cmd == 0xfb) //amplitude delay
          {
            if (restbytes < 1)
            {
              throw Error(THIS_LINE, ERROR_INVALID_FORMAT);//no details
            }
            const uint_t step = data[cur->Offset++];
            channel->Commands.push_back(ASCTrack::Command(AMPLITUDE_SLIDE, step & 31, step & 32 ? -1 : 1));
          }
        }
        cur->Counter = cur->Period;
      }
    }
  };

  struct ASCChannelState
  {
    ASCChannelState()
      : Enabled(false), Envelope(false), EnvelopeTone(0)
      , Volume(15), VolumeAddon(0), VolSlideDelay(0), VolSlideAddon(), VolSlideCounter(0)
      , BaseNoise(0), CurrentNoise(0)
      , Note(0), NoteAddon(0)
      , SampleNum(0), CurrentSampleNum(0), PosInSample(0)
      , OrnamentNum(0), CurrentOrnamentNum(0), PosInOrnament(0)
      , ToneDeviation(0)
      , SlidingSteps(0), Sliding(0), SlidingTargetNote(LIMITER), Glissade(0)
    {
    }
    bool Enabled;
    bool Envelope;
    uint_t EnvelopeTone;
    uint_t Volume;
    int_t VolumeAddon;
    uint_t VolSlideDelay;
    int_t VolSlideAddon;
    uint_t VolSlideCounter;
    int_t BaseNoise;
    int_t CurrentNoise;
    uint_t Note;
    int_t NoteAddon;
    uint_t SampleNum;
    uint_t CurrentSampleNum;
    uint_t PosInSample;
    uint_t OrnamentNum;
    uint_t CurrentOrnamentNum;
    uint_t PosInOrnament;
    int_t ToneDeviation;
    int_t SlidingSteps;//may be infinite (negative)
    int_t Sliding;
    uint_t SlidingTargetNote;
    int_t Glissade;

    void ResetBaseNoise()
    {
      BaseNoise = 0;
    }
  };

  class ASCDataRenderer : public AYM::DataRenderer
  {
  public:
    explicit ASCDataRenderer(ASCTrack::ModuleData::Ptr data)
      : Data(data)
    {
    }

    virtual void Reset()
    {
      std::fill(PlayerState.begin(), PlayerState.end(), ASCChannelState());
    }

    virtual void SynthesizeData(const TrackState& state, AYM::TrackBuilder& track)
    {
      uint_t breakSamples = 0;
      if (0 == state.Quirk())
      {
        GetNewLineState(state, track, breakSamples);
      }
      SynthesizeChannelsData(track, breakSamples);
    }
  private:
    void GetNewLineState(const TrackState& state, AYM::TrackBuilder& track, uint_t& breakSamples)
    {
      if (0 == state.Line())
      {
        std::for_each(PlayerState.begin(), PlayerState.end(), std::mem_fun_ref(&ASCChannelState::ResetBaseNoise));
      }
      if (const ASCTrack::Line* line = Data->Patterns[state.Pattern()].GetLine(state.Line()))
      {
        for (uint_t chan = 0; chan != line->Channels.size(); ++chan)
        {
          const ASCTrack::Line::Chan& src = line->Channels[chan];
          if (src.Empty())
          {
            continue;
          }
          GetNewChannelState(src, PlayerState[chan], track);
          if (src.FindCommand(BREAK_SAMPLE))
          {
            breakSamples |= 1 << chan;
          }
        }
      }
    }

    void GetNewChannelState(const ASCTrack::Line::Chan& src, ASCChannelState& dst, AYM::TrackBuilder& track)
    {
      if (src.Enabled)
      {
        dst.Enabled = *src.Enabled;
      }
      dst.VolSlideCounter = 0;
      dst.SlidingSteps = 0;
      bool contSample = false, contOrnament = false;
      for (ASCTrack::CommandsArray::const_iterator it = src.Commands.begin(), lim = src.Commands.end();
        it != lim; ++it)
      {
        switch (it->Type)
        {
        case ENVELOPE:
          if (-1 != it->Param1)
          {
            track.SetEnvelopeType(it->Param1);
          }
          if (-1 != it->Param2)
          {
            dst.EnvelopeTone = it->Param2;
            track.SetEnvelopeTone(dst.EnvelopeTone);
          }
          break;
        case ENVELOPE_ON:
          dst.Envelope = true;
          break;
        case ENVELOPE_OFF:
          dst.Envelope = false;
          break;
        case NOISE:
          dst.BaseNoise = it->Param1;
          break;
        case CONT_SAMPLE:
          contSample = true;
          break;
        case CONT_ORNAMENT:
          contOrnament = true;
          break;
        case GLISS:
          dst.Glissade = it->Param1;
          dst.SlidingSteps = -1;//infinite sliding
          break;
        case SLIDE:
        {
          dst.SlidingSteps = it->Param1;
          const int_t newSliding = (dst.Sliding | 0xf) ^ 0xf;
          dst.Glissade = -newSliding / dst.SlidingSteps;
          dst.Sliding = dst.Glissade * dst.SlidingSteps;
          break;
        }
        case SLIDE_NOTE:
        {
          dst.SlidingSteps = it->Param1;
          dst.SlidingTargetNote = it->Param2;
          const int_t absoluteSliding = track.GetSlidingDifference(dst.Note, dst.SlidingTargetNote);
          const int_t newSliding = absoluteSliding - (contSample ? dst.Sliding / 16 : 0);
          dst.Glissade = 16 * newSliding / dst.SlidingSteps;
          break;
        }
        case AMPLITUDE_SLIDE:
          dst.VolSlideCounter = dst.VolSlideDelay = it->Param1;
          dst.VolSlideAddon = it->Param2;
          break;
        case BREAK_SAMPLE:
          break;
        default:
          assert(!"Invalid cmd");
          break;
        }
      }
      if (src.OrnamentNum)
      {
        dst.OrnamentNum = *src.OrnamentNum;
      }
      if (src.SampleNum)
      {
        dst.SampleNum = *src.SampleNum;
      }
      if (src.Note)
      {
        dst.Note = *src.Note;
        dst.CurrentNoise = dst.BaseNoise;
        if (dst.SlidingSteps <= 0)
        {
          dst.Sliding = 0;
        }
        if (!contSample)
        {
          dst.CurrentSampleNum = dst.SampleNum;
          dst.PosInSample = 0;
          dst.VolumeAddon = 0;
          dst.ToneDeviation = 0;
        }
        if (!contOrnament)
        {
          dst.CurrentOrnamentNum = dst.OrnamentNum;
          dst.PosInOrnament = 0;
          dst.NoteAddon = 0;
        }
      }
      if (src.Volume)
      {
        dst.Volume = *src.Volume;
      }
    }

    void SynthesizeChannelsData(AYM::TrackBuilder& track, uint_t breakSamples)
    {
      for (uint_t chan = 0; chan != PlayerState.size(); ++chan)
      {
        const bool breakSample = 0 != (breakSamples & (1 << chan));
        AYM::ChannelBuilder channel = track.GetChannel(chan);
        SynthesizeChannel(PlayerState[chan], breakSample, channel, track);
      }
    }

    void SynthesizeChannel(ASCChannelState& dst, bool breakSample, AYM::ChannelBuilder& channel, AYM::TrackBuilder& track)
    {
      if (!dst.Enabled)
      {
        channel.SetLevel(0);
        return;
      }

      const Sample& curSample = Data->Samples[dst.CurrentSampleNum];
      const Sample::Line& curSampleLine = curSample.GetLine(dst.PosInSample);
      const Ornament& curOrnament = Data->Ornaments[dst.CurrentOrnamentNum];
      const Ornament::Line& curOrnamentLine = curOrnament.GetLine(dst.PosInOrnament);

      //calculate volume addon
      if (dst.VolSlideCounter >= 2)
      {
        dst.VolSlideCounter--;
      }
      else if (dst.VolSlideCounter)
      {
        dst.VolumeAddon += dst.VolSlideAddon;
        dst.VolSlideCounter = dst.VolSlideDelay;
      }
      if (ASCSample::INCVOLADD == curSampleLine.Command)
      {
        dst.VolumeAddon++;
      }
      else if (ASCSample::DECVOLADD == curSampleLine.Command)
      {
        dst.VolumeAddon--;
      }
      dst.VolumeAddon = clamp<int_t>(dst.VolumeAddon, -15, 15);

      //calculate tone
      dst.ToneDeviation += curSampleLine.ToneDeviation;
      dst.NoteAddon += curOrnamentLine.NoteAddon;
      const int_t halfTone = int_t(dst.Note) + dst.NoteAddon;
      const int_t toneAddon = dst.ToneDeviation + dst.Sliding / 16;
      //apply tone
      channel.SetTone(halfTone, toneAddon);

      //apply level
      channel.SetLevel((dst.Volume + 1) * clamp<int_t>(dst.VolumeAddon + curSampleLine.Level, 0, 15) / 16);
      //apply envelope
      const bool sampleEnvelope = ASCSample::ENVELOPE == curSampleLine.Command;
      if (dst.Envelope && sampleEnvelope)
      {
        channel.EnableEnvelope();
      }

      //calculate noise
      dst.CurrentNoise += curOrnamentLine.NoiseAddon;

      //mixer
      if (curSampleLine.ToneMask)
      {
        channel.DisableTone();
      }
      if (curSampleLine.NoiseMask && sampleEnvelope)
      {
        dst.EnvelopeTone += curSampleLine.Adding;
        track.SetEnvelopeTone(dst.EnvelopeTone);
      }
      else
      {
        dst.CurrentNoise += curSampleLine.Adding;
      }

      if (!curSampleLine.NoiseMask)
      {
        track.SetNoise((dst.CurrentNoise + dst.Sliding / 256) & 0x1f);
      }
      else
      {
        channel.DisableNoise();
      }

      //recalc positions
      if (dst.SlidingSteps != 0)
      {
        if (dst.SlidingSteps > 0)
        {
          if (!--dst.SlidingSteps &&
              LIMITER != dst.SlidingTargetNote) //finish slide to note
          {
            dst.Note = dst.SlidingTargetNote;
            dst.SlidingTargetNote = LIMITER;
            dst.Sliding = dst.Glissade = 0;
          }
        }
        dst.Sliding += dst.Glissade;
      }

      if (dst.PosInSample++ >= curSample.GetLoopLimit())
      {
        if (!breakSample)
        {
          dst.PosInSample = curSample.GetLoop();
        }
        else if (dst.PosInSample >= curSample.GetSize())
        {
          dst.Enabled = false;
        }
      }
      if (dst.PosInOrnament++ >= curOrnament.GetLoopLimit())
      {
        dst.PosInOrnament = curOrnament.GetLoop();
      }
    }
  private:
    const ASCTrack::ModuleData::Ptr Data;
    boost::array<ASCChannelState, Devices::AYM::CHANNELS> PlayerState;
  };

  class ASCChiptune : public AYM::Chiptune
  {
  public:
    ASCChiptune(ASCTrack::ModuleData::Ptr data, ModuleProperties::Ptr properties)
      : Data(data)
      , Properties(properties)
      , Info(CreateTrackInfo(Data, Devices::AYM::CHANNELS))
    {
    }

    virtual Information::Ptr GetInformation() const
    {
      return Info;
    }

    virtual ModuleProperties::Ptr GetProperties() const
    {
      return Properties;
    }

    virtual AYM::DataIterator::Ptr CreateDataIterator(AYM::TrackParameters::Ptr trackParams) const
    {
      const StateIterator::Ptr iterator = CreateTrackStateIterator(Info, Data);
      const AYM::DataRenderer::Ptr renderer = boost::make_shared<ASCDataRenderer>(Data);
      return AYM::CreateDataIterator(trackParams, iterator, renderer);
    }
  private:
    const ASCTrack::ModuleData::Ptr Data;
    const ModuleProperties::Ptr Properties;
    const Information::Ptr Info;
  };

  //////////////////////////////////////////////////////////////////////////
  bool CheckASCModule(const uint8_t* data, std::size_t size)
  {
    if (size < sizeof(ASCHeader))
    {
      return false;
    }

    const std::size_t limit = std::min(size, MAX_MODULE_SIZE);
    const ASCHeader* const header = safe_ptr_cast<const ASCHeader*>(data);
    const std::size_t headerBusy = sizeof(*header) + header->Length - 1;

    if (!header->Length || limit < headerBusy)
    {
      return false;
    }

    const std::size_t samplesOffset = fromLE(header->SamplesOffset);
    const std::size_t ornamentsOffset = fromLE(header->OrnamentsOffset);
    const std::size_t patternsOffset = fromLE(header->PatternsOffset);
    const uint_t patternsCount = 1 + *std::max_element(header->Positions, header->Positions + header->Length);

    //check patterns count and length
    if (!patternsCount || patternsCount > MAX_PATTERNS_COUNT ||
        header->Positions + header->Length !=
        std::find_if(header->Positions, header->Positions + header->Length,
          std::bind2nd(std::greater_equal<uint_t>(), patternsCount)))
    {
      return false;
    }

    RangeChecker::Ptr checker = RangeChecker::Create(limit);
    checker->AddRange(0, headerBusy);
    //check common ranges
    if (!checker->AddRange(patternsOffset, sizeof(ASCPattern) * patternsCount) ||
        !checker->AddRange(samplesOffset, sizeof(ASCSamples)) ||
        !checker->AddRange(ornamentsOffset, sizeof(ASCOrnaments))
        )
    {
      return false;
    }
    //check samples
    {
      const ASCSamples* const samples = safe_ptr_cast<const ASCSamples*>(data + samplesOffset);
      if (samples->Offsets.end() !=
        std::find_if(samples->Offsets.begin(), samples->Offsets.end(),
          !boost::bind(&RangeChecker::AddRange, checker.get(),
             boost::bind(std::plus<std::size_t>(), samplesOffset, boost::bind(&fromLE<uint16_t>, _1)),
             sizeof(ASCSample::Line))))
      {
        return false;
      }
    }
    //check ornaments
    {
      const ASCOrnaments* const ornaments = safe_ptr_cast<const ASCOrnaments*>(data + ornamentsOffset);
      if (ornaments->Offsets.end() !=
        std::find_if(ornaments->Offsets.begin(), ornaments->Offsets.end(),
          !boost::bind(&RangeChecker::AddRange, checker.get(),
            boost::bind(std::plus<std::size_t>(), ornamentsOffset, boost::bind(&fromLE<uint16_t>, _1)),
            sizeof(ASCOrnament::Line))))
      {
        return false;
      }
    }
    //check patterns
    {
      const ASCPattern* pattern = safe_ptr_cast<const ASCPattern*>(data + patternsOffset);
      for (uint_t patternNum = 0 ; patternNum < patternsCount; ++patternNum, ++pattern)
      {
        //at least 1 byte
        if (pattern->Offsets.end() !=
          std::find_if(pattern->Offsets.begin(), pattern->Offsets.end(),
          !boost::bind(&RangeChecker::AddRange, checker.get(),
             boost::bind(std::plus<std::size_t>(), patternsOffset, boost::bind(&fromLE<uint16_t>, _1)),
             1)))
        {
          return false;
        }
      }
    }
    return true;
  }
}

namespace
{
  using namespace ZXTune;

  //plugin attributes
  const Char ID[] = {'A', 'S', 'C', 0};
  const Char* const INFO = Text::ASC_PLUGIN_INFO;
  const uint_t CAPS = CAP_STOR_MODULE | CAP_DEV_AYM | CAP_CONV_RAW | GetSupportedAYMFormatConvertors();


  const std::string ASC_FORMAT(
    "03-32"         // uint8_t Tempo; 3..50
    "00-63"         // uint8_t Loop; 0..99
    "?00-38"        // uint16_t PatternsOffset; 0..MAX_MODULE_SIZE
    "?00-38"        // uint16_t SamplesOffset; 0..MAX_MODULE_SIZE
    "?00-38"        // uint16_t OrnamentsOffset; 0..MAX_MODULE_SIZE
    "01-64"         // uint8_t Length; 1..100
    "00-1f"         // uint8_t Positions[1]; 0..31
  );

  //////////////////////////////////////////////////////////////////////////
  class ASCModulesFactory : public ModulesFactory
  {
  public:
    ASCModulesFactory()
      : Format(Binary::Format::Create(ASC_FORMAT))
    {
    }

    virtual bool Check(const Binary::Container& inputData) const
    {
      const uint8_t* const data = static_cast<const uint8_t*>(inputData.Data());
      const std::size_t limit = inputData.Size();
      return Format->Match(data, limit) && CheckASCModule(data, limit);
    }

    virtual Binary::Format::Ptr GetFormat() const
    {
      return Format;
    }

    virtual Holder::Ptr CreateModule(ModuleProperties::RWPtr properties, Parameters::Accessor::Ptr parameters, Binary::Container::Ptr rawData, std::size_t& usedSize) const
    {
      try
      {
        assert(Check(*rawData));

        //assume all data is correct
        const IO::FastDump& data = IO::FastDump(*rawData, 0, MAX_MODULE_SIZE);

        const ASCModuleData::Ptr parsedData = boost::make_shared<ASCModuleData>();

        parsedData->ParseInformation(data, *properties);
        const std::size_t endOfSamples = parsedData->ParseSamples(data);
        const std::size_t endOfOrnaments = parsedData->ParseOrnaments(data);
        const std::size_t endOfPatterns = parsedData->ParsePatterns(data);

        //fill region
        const std::size_t rawSize = std::max(std::max(endOfSamples, endOfOrnaments), endOfPatterns);
        usedSize = std::min(rawSize, data.Size());

        //meta properties
        {
          const ASCHeader* const header = safe_ptr_cast<const ASCHeader*>(&data[0]);
          const ASCID* const id = safe_ptr_cast<const ASCID*>(header->Positions + header->Length);
          const std::size_t fixedOffset = (safe_ptr_cast<const uint8_t*>(id) - safe_ptr_cast<const uint8_t*>(header))
            + (id->Check() ? sizeof(*id) : 0);
          const ModuleRegion fixedRegion(fixedOffset, usedSize - fixedOffset);
          properties->SetSource(usedSize, fixedRegion);
        }

        const AYM::Chiptune::Ptr chiptune = boost::make_shared<ASCChiptune>(parsedData, properties);
        return AYM::CreateHolder(chiptune, parameters);
      }
      catch (const Error&/*e*/)
      {
        Log::Debug("Core::ASCSupp", "Failed to create holder");
      }
      return Holder::Ptr();
    }
  private:
    const Binary::Format::Ptr Format;
  };
}

namespace ZXTune
{
  void RegisterASCSupport(PluginsRegistrator& registrator)
  {
    const ModulesFactory::Ptr factory = boost::make_shared<ASCModulesFactory>();
    const PlayerPlugin::Ptr plugin = CreatePlayerPlugin(ID, INFO, CAPS, factory);
    registrator.RegisterPlugin(plugin);
  }
}
