/*
Abstract:
  Modules player interface definition

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/
#ifndef __MODULE_PLAYER_H_DEFINED__
#define __MODULE_PLAYER_H_DEFINED__

namespace ZXTune
{
  //forward declarations
  namespace IO
  {
    class DataContainer;
  }
  
  namespace Sound
  {
    struct Parameters;
  }
  
  namespace Module
  {
    namespace Conversion
    {
      struct Parameter;
    }
    
    /// Player interface
    class Player
    {
    public:
      typedef std::auto_ptr<Player> Ptr;

      virtual ~Player() {}

      /// Current player information
      struct Information
      {
        Information() : Capabilities()
        {
        }
        uint32_t Capabilities;
        ParametersMap Properties;
      };

      /// Module playing state
      enum PlaybackState
      {
        MODULE_PLAYING,
        MODULE_STOPPED
      };

      /// Retrieving player info itself
      virtual void GetPlayerInfo(Player::Information& info) const = 0;

      /// Retrieving information about loaded module
      virtual void GetModuleInformation(Module::Information& info) const = 0;

      /// Retrieving current state of loaded module
      virtual Error GetModuleState(unsigned& timeState, //current frame
                                   Module::Tracking& trackState, //current track position
                                   Analyze::ChannelsState& analyzeState //current analyzed state
                                   ) const = 0;

      /// Rendering frame
      virtual Error RenderFrame(const Sound::Parameters& params, //parameters for rendering
                                PlaybackState& state, //playback state
                                Sound::MultichannelReceiver& receiver //sound stream reciever
                                ) = 0;

      /// Controlling
      virtual Error Reset() = 0;
      virtual Error SetPosition(unsigned frame) = 0;

      /// Converting
      virtual Error Convert(const Conversion::Parameter& param, Dump& dst) const = 0;
    };
  }
    
  /// Enumeration
  void GetSupportedPlayers(std::vector<Module::Player::Information>& players);
  
  /// Creating
  struct DetectParameters
  {
    boost::function<bool(const Module::Player::Information&)> Filter;
    String Subpath;
    boost::function<Error(const String&, Module::Player::Ptr player)> Callback;
  };
  
  Error DetectModules(const IO::DataContainer& data, const DetectParameters& params);
}

#endif //__MODULE_PLAYER_H_DEFINED__
