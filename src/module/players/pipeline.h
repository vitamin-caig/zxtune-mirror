/**
* 
* @file
*
* @brief  Sound pipeline support
*
* @author vitamin.caig@gmail.com
*
**/

#pragma once

//library includes
#include <module/renderer.h>
#include <parameters/accessor.h>

namespace Module
{
  class Holder;

  /* Creates wrapper that applies:
   - gain
   - fadein/fadeout
   - silence detection

   Samplerate is taken from globalParams.
   Other properties are taken from globalParams, holder.Parameters in specified order
  */
  Renderer::Ptr CreatePipelinedRenderer(const Holder& holder, Parameters::Accessor::Ptr globalParams);
}
