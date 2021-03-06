/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================Plus=header=end*/

#include "PlusConfigure.h"

#include "FidLabeling.h"

#include "float.h"

#include <algorithm>

#include "PlusMath.h"

#include "vtkTriangle.h"
#include "vtkPlane.h"

//#define DEBUG_LABELING
//-----------------------------------------------------------------------------
FidLabeling::FidLabeling()
: m_ApproximateSpacingMmPerPixel(-1.0)
, m_MaxAngleDiff(-1.0)
, m_MinLinePairDistMm(-1.0)
, m_MaxLinePairDistMm(-1.0)
, m_MinLinePairAngleRad(-1.0)
, m_MaxLinePairAngleRad(-1.0)
, m_MaxLineShiftMm(10.0)
, m_MaxLinePairDistanceErrorPercent(-1.0)
, m_MinThetaRad(-1.0)
, m_MaxThetaRad(-1.0)
, m_DotsFound(false)
, m_AngleToleranceRad(-1.0)
, m_InclinedLineAngleRad(-1.0)
, m_PatternIntensity(-1.0)
{
  memset(m_FrameSize, 0, sizeof(int[2]));
}

//-----------------------------------------------------------------------------

FidLabeling::~FidLabeling()
{
}

//-----------------------------------------------------------------------------

void FidLabeling::UpdateParameters()
{
  LOG_TRACE("FidLabeling::UpdateParameters");

  // Distance between lines (= distance between planes of the N-wires)
  int numOfPatterns = m_Patterns.size();
  double epsilon = 0.001;

  // Compute normal of each pattern and evaluate the other wire endpoints if they are on the computed plane
  std::vector<vtkSmartPointer<vtkPlane> > planes;
  for (int i=0; i<numOfPatterns; ++i) 
  {
    double normal[3]={0,0,0};
    vtkTriangle::ComputeNormal(m_Patterns[i]->Wires[0].EndPointFront, m_Patterns[i]->Wires[0].EndPointBack, m_Patterns[i]->Wires[2].EndPointFront, normal);

    vtkSmartPointer<vtkPlane> plane = vtkSmartPointer<vtkPlane>::New();
    plane->SetNormal(normal);
    plane->SetOrigin(m_Patterns[i]->Wires[0].EndPointFront);
    planes.push_back(plane);

    double distance1F = plane->DistanceToPlane(m_Patterns[i]->Wires[1].EndPointFront);
    double distance1B = plane->DistanceToPlane(m_Patterns[i]->Wires[1].EndPointBack);
    double distance2B = plane->DistanceToPlane(m_Patterns[i]->Wires[2].EndPointBack);

    if (distance1F > epsilon || distance1B > epsilon || distance2B > epsilon)
    {
      LOG_ERROR("Pattern number " << i << " is invalid: the endpoints are not on the same plane");
    }
  }
  // Compute distances between each NWire pairs and determine the smallest and the largest distance
  double maxNPlaneDistance = -1.0;
  double minNPlaneDistance = FLT_MAX;
  m_MinLinePairAngleRad = vtkMath::Pi()/2.0;   
  m_MaxLinePairAngleRad = 0;
  for (int i=numOfPatterns-1; i>0; --i) 
  {
    for (int j=i-1; j>=0; --j) 
    {
      double distance = planes.at(i)->DistanceToPlane(planes.at(j)->GetOrigin());
      if (maxNPlaneDistance < distance) 
      {
        maxNPlaneDistance = distance;
      }
      if (minNPlaneDistance > distance) 
      {
        minNPlaneDistance = distance;
      }
      double angle = acos(vtkMath::Dot(planes.at(i)->GetNormal(),planes.at(j)->GetNormal())
        /vtkMath::Norm(planes.at(i)->GetNormal())
        /vtkMath::Norm(planes.at(j)->GetNormal()));
      // Normalize between -pi/2 .. +pi/2
      if (angle>vtkMath::Pi()/2)
      {
        angle -= vtkMath::Pi();
      }
      else if (angle<-vtkMath::Pi()/2)
      {
        angle += vtkMath::Pi();
      }
      // Return the absolute value (0..+pi/2)
      angle=fabs(angle);
      if (angle < m_MinLinePairAngleRad) 
      {
        m_MinLinePairAngleRad = angle;
      }
      if (angle > m_MaxLinePairAngleRad) 
      {
        m_MaxLinePairAngleRad = angle;
      }
    }
  }

  m_MaxLinePairDistMm = maxNPlaneDistance * (1.0 + (m_MaxLinePairDistanceErrorPercent / 100.0));
  m_MinLinePairDistMm = minNPlaneDistance * (1.0 - (m_MaxLinePairDistanceErrorPercent / 100.0));
  LOG_DEBUG("Line pair distance - computed min: " << minNPlaneDistance << " , max: " << maxNPlaneDistance << ";  allowed min: " << m_MinLinePairDistMm << ", max: " << m_MaxLinePairDistMm);
}

//-----------------------------------------------------------------------------

PlusStatus FidLabeling::ReadConfiguration( vtkXMLDataElement* configData, double minThetaRad, double maxThetaRad )
{
  LOG_TRACE("FidLabeling::ReadConfiguration");

  XML_FIND_NESTED_ELEMENT_REQUIRED(segmentationParameters, configData, "Segmentation");

  XML_READ_SCALAR_ATTRIBUTE_WARNING(double, ApproximateSpacingMmPerPixel, segmentationParameters);


  //if the tolerance parameters are computed automatically
  int computeSegmentationParametersFromPhantomDefinition(0);
  if(segmentationParameters->GetScalarAttribute("ComputeSegmentationParametersFromPhantomDefinition", computeSegmentationParametersFromPhantomDefinition)
    && computeSegmentationParametersFromPhantomDefinition!=0 )
  {
    LOG_WARNING("Automatic computation of the MaxLinePairDistanceErrorPercent and MaxAngleDifferenceDegrees parameters are not yet supported, use the values that are in the config file");
  }

  XML_READ_SCALAR_ATTRIBUTE_WARNING(double, MaxLinePairDistanceErrorPercent, segmentationParameters);
  XML_READ_SCALAR_ATTRIBUTE_WARNING(double, MaxAngleDifferenceDegrees, segmentationParameters);
  XML_READ_SCALAR_ATTRIBUTE_WARNING(double, AngleToleranceDegrees, segmentationParameters);
  XML_READ_SCALAR_ATTRIBUTE_WARNING(double, MaxLineShiftMm, segmentationParameters);

  XML_READ_SCALAR_ATTRIBUTE_OPTIONAL(double, InclinedLineAngleDegrees, segmentationParameters); // only for CIRS

  UpdateParameters();

  m_MinThetaRad = minThetaRad;
  m_MaxThetaRad = maxThetaRad;

  return PLUS_SUCCESS; 
}

//-----------------------------------------------------------------------------

void FidLabeling::Clear()
{
  //LOG_TRACE("FidLabeling::Clear");
  m_DotsVector.clear();
  m_LinesVector.clear();
  m_FoundDotsCoordinateValue.clear();
  m_Results.clear();
  m_FoundLines.clear();

  std::vector<FidLine> emptyLine;
  m_LinesVector.push_back(emptyLine);//initializing the 0 vector of lines (unused)
  m_LinesVector.push_back(emptyLine);//initializing the 1 vector of lines (unused)
}

//-----------------------------------------------------------------------------

void FidLabeling::SetFrameSize(int frameSize[2])
{
  LOG_TRACE("FidLineFinder::SetFrameSize(" << frameSize[0] << ", " << frameSize[1] << ")");

  if ((m_FrameSize[0] == frameSize[0]) && (m_FrameSize[1] == frameSize[1]))
  {
    return;
  }

  m_FrameSize[0] = frameSize[0];
  m_FrameSize[1] = frameSize[1];

  if (m_FrameSize[0] < 0 || m_FrameSize[1] < 0)
  {
    LOG_ERROR("Dimensions of the frame size are not positive!");
    return;
  }
}

//-----------------------------------------------------------------------------

void FidLabeling::SetAngleToleranceDeg(double value)
{
  m_AngleToleranceRad = vtkMath::RadiansFromDegrees(value);
}

//-----------------------------------------------------------------------------

bool FidLabeling::SortCompare(const std::vector<double>& temporaryLine1, const std::vector<double>& temporaryLine2)
{
  //used for SortPointsByDistanceFromOrigin
  return temporaryLine1[1] < temporaryLine2[1];
}

//-----------------------------------------------------------------------------

FidLine FidLabeling::SortPointsByDistanceFromStartPoint(FidLine& fiducials) 
{
  std::vector<std::vector<double> > temporaryLine;
  FidDot startPointIndex = m_DotsVector[fiducials.GetStartPointIndex()];

  for(unsigned int i=0 ; i<fiducials.GetNumberOfPoints() ; i++)
  {
    std::vector<double> temp;
    FidDot point = m_DotsVector[fiducials.GetPoint(i)];
    temp.push_back(fiducials.GetPoint(i));
    temp.push_back(sqrt((startPointIndex.GetX()-point.GetX())*(startPointIndex.GetX()-point.GetX()) + (startPointIndex.GetY()-point.GetY())*(startPointIndex.GetY()-point.GetY())));
    temporaryLine.push_back(temp);
  }

  //sort the indexes by the distance of their respective pioint to the startPointIndex
  std::sort(temporaryLine.begin(),temporaryLine.end(), FidLabeling::SortCompare); 

  FidLine resultLine = fiducials;

  for(unsigned int i=0 ; i<fiducials.GetNumberOfPoints() ; i++)
  {
    resultLine.SetPoint(i,temporaryLine[i][0]);
  }

  return resultLine;
}

//-----------------------------------------------------------------------------

double FidLabeling::ComputeSlope( FidLine& line )
{
  //LOG_TRACE("FidLineFinder::ComputeSlope");
  FidDot dot1 = m_DotsVector[line.GetStartPointIndex()];
  FidDot dot2 = m_DotsVector[line.GetEndPointIndex()];

  double x1 = dot1.GetX();
  double y1 = dot1.GetY();

  double x2 = dot2.GetX();
  double y2 = dot2.GetY();

  double y = (y2 - y1);
  double x = (x2 - x1);

  double t;
  if ( fabsf(x) > fabsf(y) )
  {
    t = vtkMath::Pi()/2 + atan( y / x );
  }
  else 
  {
    double tanTheta = x / y;
    if ( tanTheta > 0 )
    {
      t = vtkMath::Pi() - atan( tanTheta );
    }
    else
    {
      t = -atan( tanTheta );
    }
  }

  assert( t >= 0 && t <= vtkMath::Pi() );
  return t;
}

//-----------------------------------------------------------------------------

double FidLabeling::ComputeDistancePointLine(FidDot& dot, FidLine& line)
{     
  double x[3], y[3], z[3];

  x[0] = m_DotsVector[line.GetStartPointIndex()].GetX();
  x[1] = m_DotsVector[line.GetStartPointIndex()].GetY();
  x[2] = 0;

  y[0] = m_DotsVector[line.GetEndPointIndex()].GetX();
  y[1] = m_DotsVector[line.GetEndPointIndex()].GetY();
  y[2] = 0;

  z[0] = dot.GetX();
  z[1] = dot.GetY();
  z[2] = 0;

  return PlusMath::ComputeDistanceLinePoint( x, y, z );
}

//-----------------------------------------------------------------------------

double FidLabeling::ComputeShift(FidLine& line1, FidLine& line2)
{
  //middle of the line 1
  double midLine1[2]=
  {
    (m_DotsVector[line1.GetStartPointIndex()].GetX()+m_DotsVector[line1.GetEndPointIndex()].GetX())/2,
    (m_DotsVector[line1.GetStartPointIndex()].GetY()+m_DotsVector[line1.GetEndPointIndex()].GetY())/2
  };
  //middle of the line 2
  double midLine2[2]=
  {
    (m_DotsVector[line2.GetStartPointIndex()].GetX()+m_DotsVector[line2.GetEndPointIndex()].GetX())/2,
    (m_DotsVector[line2.GetStartPointIndex()].GetY()+m_DotsVector[line2.GetEndPointIndex()].GetY())/2
  };
  //vector from one middle to the other
  double midLine1_to_midLine2[3]=
  {
    midLine2[0]-midLine1[0],
    midLine2[1]-midLine1[1],
    0
  };

  double line1vector[3]=
  {
    m_DotsVector[line1.GetEndPointIndex()].GetX()-m_DotsVector[line1.GetStartPointIndex()].GetX(),
    m_DotsVector[line1.GetEndPointIndex()].GetY()-m_DotsVector[line1.GetStartPointIndex()].GetY(),
    0
  };
  vtkMath::Normalize(line1vector);//need to normalize for the dot product to provide significant result

  double midLine1_to_midLine2_projectionToLine1_length = vtkMath::Dot(line1vector,midLine1_to_midLine2);

  return midLine1_to_midLine2_projectionToLine1_length;
}

//-----------------------------------------------------------------------------

void FidLabeling::UpdateNWiresResults(std::vector<FidLine*>& resultLines)
{
  int numberOfLines = m_Patterns.size(); //the number of lines in the pattern
  double intensity = 0;
  std::vector<double> dotCoords;
  std::vector< std::vector<double> > foundDotsCoordinateValues = m_FoundDotsCoordinateValue;

  for (int i=0; i<numberOfLines; ++i)
  {
    SortRightToLeft(*resultLines[i]);
  }

  for (int line=0; line<numberOfLines; ++line)
  {
    for(unsigned int i=0 ; i<resultLines[line]->GetNumberOfPoints() ; i++)
    {
      LabelingResults result;
      result.x = m_DotsVector[resultLines[line]->GetPoint(i)].GetX();
      dotCoords.push_back(result.x);
      result.y = m_DotsVector[resultLines[line]->GetPoint(i)].GetY();
      dotCoords.push_back(result.y);
      result.patternId = 0;
      result.wireId = i;
      m_Results.push_back(result);
      foundDotsCoordinateValues.push_back(dotCoords);
      dotCoords.clear();
    }

    intensity += resultLines[line]->GetIntensity();

    m_FoundLines.push_back(*(resultLines[line]));
  }

  m_FoundDotsCoordinateValue = foundDotsCoordinateValues;
  m_PatternIntensity = intensity;
  m_DotsFound = true;
}

//-----------------------------------------------------------------------------

void FidLabeling::UpdateCirsResults(const FidLine& resultLine1, const FidLine& resultLine2, const FidLine& resultLine3)
{
  //resultLine1 is the left line, resultLine2 is the diagonal, resultLine3 is the right line
  double intensity = 0;
  std::vector<double> dotCoords;
  std::vector< std::vector<double> > foundDotsCoordinateValues = m_FoundDotsCoordinateValue;

  for(unsigned int i=0 ; i<resultLine1.GetNumberOfPoints() ; i++)
  {
    LabelingResults result;
    result.x = m_DotsVector[resultLine1.GetPoint(i)].GetX();
    dotCoords.push_back(result.x);
    result.y = m_DotsVector[resultLine1.GetPoint(i)].GetY();
    dotCoords.push_back(result.y);
    result.patternId = 0;
    result.wireId = i;
    m_Results.push_back(result);
    foundDotsCoordinateValues.push_back(dotCoords);
    dotCoords.clear();
  }
  intensity += resultLine1.GetIntensity();

  for(unsigned int i=0 ; i<resultLine2.GetNumberOfPoints() ; i++)
  {
    LabelingResults result;
    result.x = m_DotsVector[resultLine2.GetPoint(i)].GetX();
    dotCoords.push_back(result.x);
    result.y = m_DotsVector[resultLine2.GetPoint(i)].GetY();
    dotCoords.push_back(result.y);
    result.patternId = 1;
    result.wireId = i;
    m_Results.push_back(result);
    foundDotsCoordinateValues.push_back(dotCoords);
    dotCoords.clear();
  }
  intensity += resultLine2.GetIntensity();

  for(unsigned int i=0 ; i<resultLine3.GetNumberOfPoints() ; i++)
  {
    LabelingResults result;
    result.x = m_DotsVector[resultLine3.GetPoint(i)].GetX();
    dotCoords.push_back(result.x);
    result.y = m_DotsVector[resultLine3.GetPoint(i)].GetY();
    dotCoords.push_back(result.y);
    result.patternId = 2;
    result.wireId = i;
    m_Results.push_back(result);
    foundDotsCoordinateValues.push_back(dotCoords);
    dotCoords.clear();
  }
  intensity += resultLine3.GetIntensity();

  m_FoundLines.push_back(resultLine1);
  m_FoundLines.push_back(resultLine2);
  m_FoundLines.push_back(resultLine3);

  m_FoundDotsCoordinateValue = foundDotsCoordinateValues;
  m_PatternIntensity = intensity;
  m_DotsFound = true;
}

//-----------------------------------------------------------------------------

void FidLabeling::FindPattern()
{
  //LOG_TRACE("FidLabeling::FindPattern");

  std::vector<FidLine> maxPointsLines = m_LinesVector[m_LinesVector.size()-1];
  int numberOfLines = m_Patterns.size();//the number of lines in the pattern
  int numberOfCandidateLines = maxPointsLines.size();
  std::vector<int> lineIndices(numberOfLines);
  std::vector<LabelingResults> results;

#ifdef DEBUG_LABELING
  LOG_DEBUG("Number of lines in the pattern: "<<numberOfLines);
  LOG_DEBUG("Number of candidate lines: "<<numberOfCandidateLines);  
  for (int lineIndex=0; lineIndex<numberOfCandidateLines; lineIndex++)
  {
    std::ostringstream linePermutationStr; 
    linePermutationStr.clear();
    std::vector<int> *pointIndices=maxPointsLines[lineIndex].GetPoints();
    for (int pointIndex=0; pointIndex<pointIndices->size(); pointIndex++)
    {
      if (pointIndex>0)
      {
        linePermutationStr << ", ";
      }
      linePermutationStr << (*pointIndices)[pointIndex];        
    }
    linePermutationStr << std::ends;    
    LOG_DEBUG("  Line "<<lineIndex<<": "<<linePermutationStr.str());
  }
#endif

  m_DotsFound = false;

  //permutation generator
  for(unsigned int i=0 ; i<lineIndices.size() ; i++)
  {
    lineIndices[i] = lineIndices.size()-1-i;
  }
  lineIndices[0]--;

  bool foundPattern = false;
  do
  {
    for (int i=0; i<numberOfLines; i++)
    {
      lineIndices[i]++;

      if (lineIndices[i]<numberOfCandidateLines-i)
      {
        // no need to carry over more
        // the i-th index is correct, so just adjust all the ones before that
        int nextIndex=lineIndices[i]+1;
        for (int j=i-1; j>=0; j--)
        {
          lineIndices[j] = nextIndex;
          nextIndex++;
        }
        break; //valid permutation
      }

      // need to carry over
      if (i==numberOfLines-1)
      {
        // we are already at the last index, cannot carry over more
        return;
      }
    }

#ifdef DEBUG_LABELING
    std::ostringstream linePermutationStr; 
    for (int lineIndex=0; lineIndex<numberOfLines; lineIndex++)
    {
      if (lineIndex>0)
      {
        linePermutationStr << ", ";
      }
      linePermutationStr << lineIndices[lineIndex];
    }
    linePermutationStr << std::ends;
    LOG_DEBUG("Try permutation: ("<<linePermutationStr.str()<<")");
#endif


    // We have a new permutation in lineIndices.
    // Check if the distance and angle between each possible line pairs within the permutation are within the allowed range.
    // This is a quick filtering to keep only those line combinations for further processing that may form a valid pattern.
    foundPattern=true; // assume that we've found a valid pattern (then set the flag to false if it turns out that one of the values are not within the allowed range)
    for( int i=0 ; i<numberOfLines-1 && foundPattern; i++)
    {
      FidLine currentLine1 = maxPointsLines[lineIndices[i]];
      for( int j=i+1 ; j<numberOfLines && foundPattern; j++)
      {
        FidLine currentLine2 = maxPointsLines[lineIndices[j]];

        double angleBetweenLinesRad = FidLine::ComputeAngleRad(currentLine1, currentLine2);
        if (angleBetweenLinesRad<m_AngleToleranceRad) //The angle between 2 lines is close to 0
        {
          // Parallel lines

          // Check the distance between the lines
          double distance = ComputeDistancePointLine(m_DotsVector[currentLine1.GetStartPointIndex()], currentLine2);
          int maxLinePairDistPx = floor(m_MaxLinePairDistMm / m_ApproximateSpacingMmPerPixel + 0.5 );
          int minLinePairDistPx = floor(m_MinLinePairDistMm / m_ApproximateSpacingMmPerPixel + 0.5 );        
          if((distance > maxLinePairDistPx) || (distance < minLinePairDistPx))
          {
            // The distance between the lines is smaller or larger than the allowed range
#ifdef DEBUG_LABELING
            LOG_DEBUG("The distance between the lines is smaller or larger than the allowed range: distance="<<distance
              <<", min="<<minLinePairDistPx<<", max="<<maxLinePairDistPx);
#endif
            foundPattern=false;
            break;
          }

          // Check the shift (along the direction of the lines)
          double shift = ComputeShift(currentLine1,currentLine2);
          int maxLineShiftDistPx = floor(m_MaxLineShiftMm / m_ApproximateSpacingMmPerPixel + 0.5 );
          //maxLineShiftDistPx = 35;
          if(fabs(shift) > maxLineShiftDistPx)
          {
            // The shift between the is larger than the allowed value
#ifdef DEBUG_LABELING
            LOG_DEBUG("The shift between the is larger than the allowed value: shift="<<shift
              <<", maxLineShiftDistPx="<<maxLineShiftDistPx);
#endif
            foundPattern=false;
            break;
          }
        }
        else 
        {
          // Non-parallel lines
#ifdef DEBUG_LABELING
          LOG_DEBUG("Non-parallel lines");
#endif
          double minAngle=m_MinLinePairAngleRad-m_AngleToleranceRad;
          double maxAngle=m_MaxLinePairAngleRad+m_AngleToleranceRad;
          if ( (angleBetweenLinesRad>maxAngle) || (angleBetweenLinesRad<minAngle) )
          {
            // The angle between the patterns are not in the valid range
#ifdef DEBUG_LABELING
            LOG_DEBUG("The angle between the patterns are not in the valid range: angleBetweenLinesRad="<<angleBetweenLinesRad
              <<", min="<<minAngle<<", max="<<maxAngle);
#endif
            foundPattern=false;
            break;
          }

          // If there are common endpoints between the lines then we check if the angle between the lines is correct
          // (Needed e.g., for the CIRS phantom model 45)
          int commonPointIndex = -1; // <0 if there are no common points between the lines, >=0 if there is a common endpoint
          if((currentLine1.GetStartPointIndex() == currentLine2.GetStartPointIndex()) || (currentLine1.GetStartPointIndex() == currentLine2.GetEndPointIndex()))
          {
            commonPointIndex = currentLine1.GetStartPointIndex();
          }
          else if((currentLine1.GetEndPointIndex() == currentLine2.GetStartPointIndex()) || (currentLine1.GetEndPointIndex() == currentLine2.GetEndPointIndex()))
          {
            commonPointIndex = currentLine1.GetEndPointIndex();
          }                    
          if(commonPointIndex != -1)          
          {
            // there is a common point
            double minAngle=m_InclinedLineAngleRad-m_AngleToleranceRad;
            double maxAngle=m_InclinedLineAngleRad+m_AngleToleranceRad;
            if ( (angleBetweenLinesRad>maxAngle) || (angleBetweenLinesRad<minAngle) )
            {
              // The angle between the patterns are not in the valid range
#ifdef DEBUG_LABELING
            LOG_DEBUG("The angle between the patterns are not in the valid range: angleBetweenLinesRad="<<angleBetweenLinesRad
              <<", min="<<minAngle<<", max="<<maxAngle);
#endif
              foundPattern=false;
              break;
            }
          }
        }
      }
    }

  } while ((lineIndices[numberOfLines-1]!=numberOfCandidateLines-numberOfLines+2) && (!foundPattern));

#ifdef DEBUG_LABELING
  LOG_DEBUG("Found a valid pattern");
#endif

  if (foundPattern)//We have the right permutation of lines in lineIndices
  {
    //Update the results, this part is not generic but depends on the FidPattern we are looking for
    NWire* nWire = dynamic_cast<NWire*>(m_Patterns.at(0));
    CoplanarParallelWires* coplanarParallelWire = dynamic_cast<CoplanarParallelWires*>(m_Patterns.at(0));
    if (nWire) // NWires
    {
      std::vector<FidLine*> resultLines(numberOfLines);
      std::vector<double> resultLineMiddlePointYs;

      for (std::vector<int>::iterator it = lineIndices.begin(); it != lineIndices.end(); ++it)
      {
        resultLineMiddlePointYs.push_back( (m_DotsVector[maxPointsLines[*it].GetStartPointIndex()].GetY()+m_DotsVector[maxPointsLines[*it].GetEndPointIndex()].GetY())/2 );
      }

      // Sort result lines according to middlePoint Y's
      // TODO: If the wire pattern is asymmetric then use the pattern geometry to match the lines to the intersection points instead of just sort them by Y value (https://www.assembla.com/spaces/plus/tickets/435)
      std::vector<double>::iterator middlePointYsBeginIt = resultLineMiddlePointYs.begin();
      for (int i=0; i<numberOfLines; ++i)
      {
        std::vector<double>::iterator middlePointYsMinIt = std::min_element(middlePointYsBeginIt, resultLineMiddlePointYs.end());
        int minIndex = (int)std::distance(middlePointYsBeginIt,middlePointYsMinIt);
        resultLines[i] = &maxPointsLines[lineIndices[minIndex]];
        (*middlePointYsMinIt) = DBL_MAX;
      }

      UpdateNWiresResults(resultLines);
    }
    else if (coplanarParallelWire) // CIRS phantom
    {
      FidLine resultLine1 = maxPointsLines[lineIndices[0]];
      FidLine resultLine2 = maxPointsLines[lineIndices[1]];
      FidLine resultLine3 = maxPointsLines[lineIndices[2]];

      bool test1 = (resultLine1.GetStartPointIndex() == resultLine2.GetStartPointIndex()) || (resultLine1.GetStartPointIndex() == resultLine2.GetEndPointIndex());
      bool test2 = (resultLine1.GetEndPointIndex() == resultLine2.GetStartPointIndex()) || (resultLine1.GetEndPointIndex() == resultLine2.GetEndPointIndex());
      bool test3 = (resultLine1.GetStartPointIndex() == resultLine3.GetStartPointIndex()) || (resultLine1.GetStartPointIndex() == resultLine3.GetEndPointIndex());
      //bool test4 = (resultLine1.GetEndPointIndex() == resultLine3.GetStartPointIndex()) || (resultLine1.GetEndPointIndex() == resultLine3.GetEndPointIndex());

      if(!test1 && !test2)//if line 1 and 2 have no point in common
      {
        if(m_DotsVector[resultLine1.GetStartPointIndex()].GetX() > m_DotsVector[resultLine2.GetStartPointIndex()].GetX())//Line 1 is on the left on the image
        {
          UpdateCirsResults(resultLine1, resultLine3, resultLine2);
        }
        else
        {
          UpdateCirsResults(resultLine2, resultLine3, resultLine1);
        }
      }
      else if(!test1 && !test3)//if line 1 and 3 have no point in common
      {
        if(m_DotsVector[resultLine1.GetStartPointIndex()].GetX() > m_DotsVector[resultLine3.GetStartPointIndex()].GetX())//Line 1 is on the left on the image
        {
          UpdateCirsResults(resultLine1, resultLine2, resultLine3);
        }
        else
        {
          UpdateCirsResults(resultLine3, resultLine2, resultLine1);
        }
      }
      else//if line 2 and 3 have no point in common
      {
        if(m_DotsVector[resultLine2.GetStartPointIndex()].GetX() > m_DotsVector[resultLine3.GetStartPointIndex()].GetX())//Line 2 is on the left on the image
        {
          UpdateCirsResults(resultLine2, resultLine1, resultLine3);
        }
        else
        {
          UpdateCirsResults(resultLine3, resultLine1, resultLine2);
        }
      }
    }   
  }
}

//-----------------------------------------------------------------------------
void FidLabeling::SortRightToLeft( FidLine& line )
{
  //LOG_TRACE("FidLabeling::SortRightToLeft");

  std::vector<std::vector<FidDot>::iterator> pointsIterator(line.GetNumberOfPoints());

  for (unsigned int i=0; i<line.GetNumberOfPoints() ; i++)
  {
    pointsIterator[i] = m_DotsVector.begin() + line.GetPoint(i);
  }

  std::sort(pointsIterator.begin(), pointsIterator.end(), FidDot::PositionLessThan);

  for (unsigned int i=0; i<line.GetNumberOfPoints(); i++)
  {
    line.SetPoint(i, pointsIterator[i] - m_DotsVector.begin());
  }
}

//-----------------------------------------------------------------------------
void FidLabeling::SetMinThetaDeg(double value) 
{ 
  m_MinThetaRad = vtkMath::RadiansFromDegrees(value); 
}

//-----------------------------------------------------------------------------
void FidLabeling::SetMaxThetaDeg(double value) 
{ 
  m_MaxThetaRad = vtkMath::RadiansFromDegrees(value); 
}

//-----------------------------------------------------------------------------
void FidLabeling::SetMaxLineShiftMm( double aValue )
{
  m_MaxLineShiftMm = aValue;
}

//-----------------------------------------------------------------------------
double FidLabeling::GetMaxLineShiftMm()
{
  return m_MaxLineShiftMm;
}

//-----------------------------------------------------------------------------
void FidLabeling::SetAngleToleranceDegrees(double angleToleranceDegrees)
{
  m_AngleToleranceRad = vtkMath::RadiansFromDegrees(angleToleranceDegrees); 
}

//-----------------------------------------------------------------------------
void FidLabeling::SetInclinedLineAngleDegrees(double inclinedLineAngleDegrees)
{
  m_InclinedLineAngleRad = vtkMath::RadiansFromDegrees(inclinedLineAngleDegrees); 
}
