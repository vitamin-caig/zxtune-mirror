/**
* 
* @file
*
* @brief  Fading support
*
* @author vitamin.caig@gmail.com
*
**/

#pragma once

//local includes
#include "module/players/streaming.h"
//library includes
#include <module/state.h>
#include <parameters/accessor.h>
#include <sound/gainer.h>

namespace Module
{
  Sound::GainSource::Ptr CreateGainSource(Parameters::Accessor::Ptr params, Time::Milliseconds duration, State::Ptr status);
  
  inline Sound::Receiver::Ptr CreateFadingReceiver(Parameters::Accessor::Ptr params, Time::Milliseconds duration, State::Ptr status, Sound::Receiver::Ptr target)
  {
    return Sound::CreateGainer(CreateGainSource(std::move(params), duration, std::move(status)), std::move(target));
  }
}
