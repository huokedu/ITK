/*=========================================================================

  Program:   Insight Segmentation & Registration Toolkit
  Module:    itkShapeDetectionLevelSetFilter.txx
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

Copyright (c) 2001 Insight Consortium
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

 * The name of the Insight Consortium, nor the names of any consortium members,
   nor of any contributors, may be used to endorse or promote products derived
   from this software without specific prior written permission.

  * Modified source versions must be plainly marked as such, and must not be
    misrepresented as being the original software.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS ``AS IS''
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=========================================================================*/
#ifndef _itkShapeDetectionLevelSetFilter_txx
#define _itkShapeDetectionLevelSetFilter_txx

#include "itkLevelSetCurvatureFunction.h"
#include "itkEntropyPreservingGradientMagnitudeImageFunction.h"
#include "itkImageRegionIterator.h"

#include "vnl/vnl_math.h"

namespace itk
{

/**
 *
 */
template <class TLevelSet, class TEdgeImage>
ShapeDetectionLevelSetFilter<TLevelSet,TEdgeImage>
::ShapeDetectionLevelSetFilter()
{
  this->ProcessObject::SetNumberOfRequiredInputs( 2 );
  m_LengthPenaltyStrength = 0.05;

  m_OutputNarrowBand = NULL;
  m_Extender = ExtenderType::New();

  m_PropagateOutwards = true;

}

/**
 *
 */
template <class TLevelSet, class TEdgeImage>
void
ShapeDetectionLevelSetFilter<TLevelSet,TEdgeImage>
::PrintSelf(std::ostream& os, Indent indent) const
{
  Superclass::PrintSelf(os,indent);
  os << indent << "Length penalty strength: ";
  os << m_LengthPenaltyStrength << std::endl;
  os << indent << "Propagate outwards: " << m_PropagateOutwards << std::endl;

}


/**
 *
 */
template <class TLevelSet, class TEdgeImage>
void
ShapeDetectionLevelSetFilter<TLevelSet,TEdgeImage>
::SetEdgeImage(EdgeImageType * ptr)
{
  this->ProcessObject::SetNthInput( 1, ptr );
}


/**
 *
 */
template <class TLevelSet, class TEdgeImage>
ShapeDetectionLevelSetFilter<TLevelSet,TEdgeImage>
::EdgeImagePointer
ShapeDetectionLevelSetFilter<TLevelSet,TEdgeImage>
::GetEdgeImage()
{
  if ( this->GetNumberOfInputs() < 2 )
    {
    return NULL;
    }

  return static_cast<TEdgeImage *>(
    this->ProcessObject::GetInput(1).GetPointer() );

}


/**
 *
 */
template <class TLevelSet, class TEdgeImage>
void
ShapeDetectionLevelSetFilter<TLevelSet,TEdgeImage>
::AllocateOutput()
{
  // allocate memory for the output image
  LevelSetPointer outputPtr = this->GetOutput();
  outputPtr->SetBufferedRegion( 
    outputPtr->GetRequestedRegion() );
  outputPtr->Allocate();

}


/**
 *
 */
template <class TLevelSet, class TEdgeImage>
void
ShapeDetectionLevelSetFilter<TLevelSet,TEdgeImage>
::GenerateInputRequestedRegion()
{
  // call the superclass implementation
  this->Superclass::GenerateInputRequestedRegion();

  // this filter requires all of the input image to
  // be in the buffer
  this->GetEdgeImage()->SetRequestedRegionToLargestPossibleRegion();

}


/**
 *
 */
template <class TLevelSet, class TEdgeImage>
void
ShapeDetectionLevelSetFilter<TLevelSet,TEdgeImage>
::GenerateData()
{

  LevelSetPointer inputPtr = this->GetInput();

  this->AllocateOutput();

  if( this->GetNarrowBanding() ) 
    {
    this->GenerateDataNarrowBand();
    }
  else
    {
    this->GenerateDataFull();
    }

}


/**
 *
 */
template <class TLevelSet, class TEdgeImage>
void
ShapeDetectionLevelSetFilter<TLevelSet,TEdgeImage>
::GenerateDataFull()
{

  this->AllocateBuffers();
  this->CopyInputToInputBuffer();

  LevelSetType::PixelType tempPixel;
  
  unsigned int numberOfIterations = this->GetNumberOfIterations();
  double timeStepSize = this->GetTimeStepSize();


  for( unsigned int k = 0; k < numberOfIterations; k++ )
    {

    // Update progress.
    if ( numberOfIterations < 100 || !(k % 10) )
      {
      this->UpdateProgress( (float) k / (float) numberOfIterations );
      }

    LevelSetPointer inputBuffer = this->GetInputBuffer();
    LevelSetPointer outputBuffer = this->GetOutputBuffer();

    // Setup the extender
    m_Extender->SetInputVelocityImage( this->GetEdgeImage() );
    m_Extender->SetInput( inputBuffer );
    m_Extender->Update();
    typename TEdgeImage::Pointer extVelPtr = m_Extender->GetOutputVelocityImage();

    // Define a level set curvature calculator
    typedef
      LevelSetCurvatureFunction<LevelSetImageType> CurvatureType;
    CurvatureType::Pointer inCurvature = CurvatureType::New();
    inCurvature->SetInputImage( inputBuffer );

    // Define a entropy-satisfying derivative calculator
    typedef
      EntropyPreservingGradientMagnitudeImageFunction<LevelSetImageType> DerivativeType;
    DerivativeType::Pointer inEntropy = DerivativeType::New();
    inEntropy->SetInputImage( inputBuffer );

      // Define iterators
    typedef
      ImageRegionIterator<LevelSetImageType> IteratorType;
  
    IteratorType inBuffIt = IteratorType( inputBuffer, 
      inputBuffer->GetBufferedRegion() );
    IteratorType outBuffIt = IteratorType( outputBuffer, 
      outputBuffer->GetBufferedRegion() );

    typedef
      ImageRegionIterator<EdgeImageType> 
        SpeedIteratorType;

    SpeedIteratorType speedIt = SpeedIteratorType( extVelPtr, 
      extVelPtr->GetBufferedRegion() );

    typedef Index<TLevelSet::ImageDimension> IndexType;

    IndexType index;
    double curvature;
    double magnitude;
    double updateValue;
    double speed;
    double value;

    while( !outBuffIt.IsAtEnd() )
      {
      index = outBuffIt.GetIndex();

      magnitude = inEntropy->EvaluateAtIndex( index );
      updateValue = -1.0 * magnitude;

      curvature = inCurvature->EvaluateAtIndex( index );
      magnitude = inCurvature->GetMagnitude();
      updateValue += m_LengthPenaltyStrength * curvature * magnitude;

      speed = (double) speedIt.Get();

      updateValue *= timeStepSize * speed; 
    
      value = (double) inBuffIt.Get();
      value += updateValue;

      tempPixel = outBuffIt.Get();
      tempPixel = value;
      outBuffIt.Set( tempPixel );

      ++outBuffIt;
      ++inBuffIt;
      ++speedIt;

      }

    this->SwapBuffers();
    }

  this->SwapBuffers();
  this->CopyOutputBufferToOutput();
                                        
}

/**
 *
 */
template <class TLevelSet, class TEdgeImage>
void
ShapeDetectionLevelSetFilter<TLevelSet,TEdgeImage>
::GenerateDataNarrowBand()
{

  this->AllocateBuffers(true);

  LevelSetPointer outputPtr = this->GetOutputBuffer();
  LevelSetPointer inputPtr = this->GetInput();

  double narrowBandwidth = this->GetNarrowBandwidth();

  // copy input to output
  typedef
     ImageRegionIterator<LevelSetImageType> IteratorType;
  
  IteratorType inIt = IteratorType( inputPtr, 
    inputPtr->GetBufferedRegion() );
  IteratorType outIt = IteratorType( outputPtr, 
    outputPtr->GetBufferedRegion() );

  while( !inIt.IsAtEnd() )
    {
    outIt.Set( inIt.Get() );
    ++inIt;
    ++outIt;
    }

  // Setup the extender
  m_Extender->SetInputVelocityImage( this->GetEdgeImage() );
  m_Extender->NarrowBandingOn();
  m_Extender->SetNarrowBandwidth( narrowBandwidth );

  NodeContainerPointer inputNarrowBand = this->GetInputNarrowBand();
  unsigned int numberOfIterations = this->GetNumberOfIterations();
  double timeStepSize = this->GetTimeStepSize();

  for( unsigned int k = 0; k < numberOfIterations; k++ )
    {

    // Update progress.
    if ( numberOfIterations < 100 || !(k % 10) )
      {
      this->UpdateProgress( (float) k / (float) numberOfIterations );
      }

    m_Extender->SetInput( outputPtr );
    m_Extender->SetInputNarrowBand( inputNarrowBand );
    m_Extender->Update();

    typename TEdgeImage::Pointer extVelPtr = m_Extender->GetOutputVelocityImage();

    inputNarrowBand = m_Extender->GetOutputNarrowBand();
    inputPtr = m_Extender->GetOutput();

    // Define a level set curvature calculator
    typedef
      LevelSetCurvatureFunction<LevelSetImageType> CurvatureType;
    CurvatureType::Pointer inCurvature = CurvatureType::New();
    inCurvature->SetInputImage( inputPtr );

    // Define a entropy-satisfying derivative calculator
    typedef
      EntropyPreservingGradientMagnitudeImageFunction<LevelSetImageType> DerivativeType;
    DerivativeType::Pointer inEntropy = DerivativeType::New();
    inEntropy->SetInputImage( inputPtr );

    typename NodeContainer::ConstIterator pointsIt;
    typename NodeContainer::ConstIterator pointsEnd;
  
    pointsIt = inputNarrowBand->Begin();
    pointsEnd = inputNarrowBand->End();

    double maxValue = narrowBandwidth / 2.0;
    NodeType node;
    PixelType lsetPixel;
    double curvature;
    double magnitude;
    double updateValue;
    double speed;
    double value;

    for( ; pointsIt != pointsEnd; ++pointsIt )
      {
      node = pointsIt.Value();

      if( vnl_math_abs( node.GetValue() ) <= maxValue )
        {
        
        magnitude = inEntropy->EvaluateAtIndex( node.GetIndex() );
        updateValue = -1.0 * magnitude;

        curvature = inCurvature->EvaluateAtIndex( node.GetIndex() );
        magnitude = inCurvature->GetMagnitude();
        updateValue += m_LengthPenaltyStrength * curvature * magnitude;

        speed = (double) extVelPtr->GetPixel(node.GetIndex());

        updateValue *= timeStepSize * speed; 
    
        value = (double) inputPtr->GetPixel( node.GetIndex() );
        value += updateValue;

        lsetPixel = value;
        outputPtr->SetPixel( node.GetIndex(), lsetPixel );

        }

      } // end while loop

    } // end iteration loop

  m_OutputNarrowBand = inputNarrowBand;
  this->CopyOutputBufferToOutput();
                                        
}


} // namespace itk

#endif
