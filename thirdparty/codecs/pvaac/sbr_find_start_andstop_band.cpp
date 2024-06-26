/* ------------------------------------------------------------------
 * Copyright (C) 1998-2009 PacketVideo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */
/*

 Filename: sbr_find_start_andstop_band.c

------------------------------------------------------------------------------
 REVISION HISTORY


 Who:                                   Date: MM/DD/YYYY
 Description:

------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS



------------------------------------------------------------------------------
 FUNCTION DESCRIPTION


------------------------------------------------------------------------------
 REQUIREMENTS


------------------------------------------------------------------------------
 REFERENCES

SC 29 Software Copyright Licencing Disclaimer:

This software module was originally developed by
  Coding Technologies

and edited by
  -

in the course of development of the ISO/IEC 13818-7 and ISO/IEC 14496-3
standards for reference purposes and its performance may not have been
optimized. This software module is an implementation of one or more tools as
specified by the ISO/IEC 13818-7 and ISO/IEC 14496-3 standards.
ISO/IEC gives users free license to this software module or modifications
thereof for use in products claiming conformance to audiovisual and
image-coding related ITU Recommendations and/or ISO/IEC International
Standards. ISO/IEC gives users the same free license to this software module or
modifications thereof for research purposes and further ISO/IEC standardisation.
Those intending to use this software module in products are advised that its
use may infringe existing patents. ISO/IEC have no liability for use of this
software module or modifications thereof. Copyright is not released for
products that do not conform to audiovisual and image-coding related ITU
Recommendations and/or ISO/IEC International Standards.
The original developer retains full right to modify and use the code for its
own purpose, assign or donate the code to a third party and to inhibit third
parties from using the code for products that do not conform to audiovisual and
image-coding related ITU Recommendations and/or ISO/IEC International Standards.
This copyright notice must be included in all copies or derivative works.
Copyright (c) ISO/IEC 2002.

------------------------------------------------------------------------------
 PSEUDO-CODE

------------------------------------------------------------------------------
*/


/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/

#ifdef LITEPLAYER_CONFIG_AAC_PLUS


#include    "sbr_find_start_andstop_band.h"
#include    "get_sbr_startfreq.h"
#include    "get_sbr_stopfreq.h"

/*----------------------------------------------------------------------------
; MACROS
; Define module specific macros here
----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
; DEFINES
; Include all pre-processor statements here. Include conditional
; compile variables also.
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; LOCAL FUNCTION DEFINITIONS
; Function Prototype declaration
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; LOCAL STORE/BUFFER/POINTER DEFINITIONS
; Variable declaration - defined here and used outside this module
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; EXTERNAL FUNCTION REFERENCES
; Declare functions defined elsewhere and referenced in this module
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; EXTERNAL GLOBAL STORE/BUFFER/POINTER REFERENCES
; Declare variables used in this module but defined elsewhere
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/

SBR_ERROR sbr_find_start_andstop_band(const Int32 samplingFreq,
                                      const Int32 startFreq,
                                      const Int32 stopFreq,
                                      Int   *lsbM,
                                      Int   *usb)
{
    /* Update startFreq struct */
    *lsbM = get_sbr_startfreq(samplingFreq, startFreq);

    if (*lsbM == 0)
    {
        return(SBRDEC_ILLEGAL_SCFACTORS);
    }

    /*Update stopFreq struct */
    if (stopFreq < 13)
    {
        *usb = get_sbr_stopfreq(samplingFreq, stopFreq);
    }
    else if (stopFreq == 13)
    {
        *usb = 64;
    }
    else if (stopFreq == 14)
    {
        *usb = (*lsbM) << 1;
    }
    else
    {
        *usb = 3 * *lsbM;
    }

    /* limit to Nyqvist */
    if (*usb > 64)
    {
        *usb = 64;
    }

    /* test for invalid lsb, usb combinations */
    if ((*usb - *lsbM) > 48)
    {
        /*
         *  invalid SBR bitstream ?
         */
        return(SBRDEC_INVALID_BITSTREAM);
    }

    if ((samplingFreq == 44100) && ((*usb - *lsbM) > 35))
    {
        /*
         *  invalid SBR bitstream ?
         */
        return(SBRDEC_INVALID_BITSTREAM);
    }

    if ((samplingFreq >= 48000) && ((*usb - *lsbM) > 32))
    {
        /*
         *  invalid SBR bitstream ?
         */
        return(SBRDEC_INVALID_BITSTREAM);
    }

    return(SBRDEC_OK);

}

#endif



