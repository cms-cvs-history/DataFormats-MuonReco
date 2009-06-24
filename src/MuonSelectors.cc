#include "DataFormats/MuonReco/interface/MuonSelectors.h"
#include "DataFormats/TrackReco/interface/Track.h"

namespace muon {
SelectionType selectionTypeFromString( const std::string &label )
{
   static SelectionTypeStringToEnum selectionTypeStringToEnumMap[] = {
      { "All", All },
      { "AllGlobalMuons", AllGlobalMuons },
      { "AllStandAloneMuons", AllStandAloneMuons },
      { "AllTrackerMuons", AllTrackerMuons },
      { "TrackerMuonArbitrated", TrackerMuonArbitrated },
      { "AllArbitrated", AllArbitrated },
      { "GlobalMuonPromptTight", GlobalMuonPromptTight },
      { "TMLastStationLoose", TMLastStationLoose },
      { "TMLastStationTight", TMLastStationTight },
      { "TM2DCompatibilityLoose", TM2DCompatibilityLoose },
      { "TM2DCompatibilityTight", TM2DCompatibilityTight },
      { "TMOneStationLoose", TMOneStationLoose },
      { "TMOneStationTight", TMOneStationTight },
      { "TMLastStationOptimizedLowPtLoose", TMLastStationOptimizedLowPtLoose },
      { "TMLastStationOptimizedLowPtTight", TMLastStationOptimizedLowPtTight },
      { 0, (SelectionType)-1 }
   };

   SelectionType value = (SelectionType)-1;
   bool found = false;
   for(int i = 0; selectionTypeStringToEnumMap[i].label && (! found); ++i)
      if (! strcmp(label.c_str(), selectionTypeStringToEnumMap[i].label)) {
         found = true;
         value = selectionTypeStringToEnumMap[i].value;
      }

   // in case of unrecognized selection type
   if (! found) throw cms::Exception("MuonSelectorError") << label << " is not a recognized SelectionType";
   return value;
}
}

unsigned int muon::RequiredStationMask( const reco::Muon& muon,
					  double maxChamberDist,
					  double maxChamberDistPull,
					  reco::Muon::ArbitrationType arbitrationType )
{
   unsigned int theMask = 0;

   for(int stationIdx = 1; stationIdx < 5; ++stationIdx)
      for(int detectorIdx = 1; detectorIdx < 3; ++detectorIdx)
         if(muon.trackDist(stationIdx,detectorIdx,arbitrationType) < maxChamberDist &&
               muon.trackDist(stationIdx,detectorIdx,arbitrationType)/muon.trackDistErr(stationIdx,detectorIdx,arbitrationType) < maxChamberDistPull)
            theMask += 1<<((stationIdx-1)+4*(detectorIdx-1));

   return theMask;
}

// ------------ method to calculate the calo compatibility for a track with matched muon info  ------------
float muon::caloCompatibility(const reco::Muon& muon) {
  return muon.caloCompatibility();
}

// ------------ method to calculate the segment compatibility for a track with matched muon info  ------------
float muon::segmentCompatibility(const reco::Muon& muon) {
  bool use_weight_regain_at_chamber_boundary = true;
  bool use_match_dist_penalty = true;

  int nr_of_stations_crossed = 0;
  int nr_of_stations_with_segment = 0;
  std::vector<int> stations_w_track(8);
  std::vector<int> station_has_segmentmatch(8);
  std::vector<int> station_was_crossed(8);
  std::vector<float> stations_w_track_at_boundary(8);
  std::vector<float> station_weight(8);
  int position_in_stations = 0;
  float full_weight = 0.;

  for(int i = 1; i<=8; ++i) {
    // ********************************************************;
    // *** fill local info for this muon (do some counting) ***;
    // ************** begin ***********************************;
    if(i<=4) { // this is the section for the DTs
      if( muon.trackDist(i,1) < 999999 ) { //current "raw" info that a track is close to a chamber
	++nr_of_stations_crossed;
	station_was_crossed[i-1] = 1;
	if(muon.trackDist(i,1) > -10. ) stations_w_track_at_boundary[i-1] = muon.trackDist(i,1); 
	else stations_w_track_at_boundary[i-1] = 0.;
      }
      if( muon.segmentX(i,1) < 999999 ) { //current "raw" info that a segment is matched to the current track
	++nr_of_stations_with_segment;
	station_has_segmentmatch[i-1] = 1;
      }
    }
    else     { // this is the section for the CSCs
      if( muon.trackDist(i-4,2) < 999999 ) { //current "raw" info that a track is close to a chamber
	++nr_of_stations_crossed;
	station_was_crossed[i-1] = 1;
	if(muon.trackDist(i-4,2) > -10. ) stations_w_track_at_boundary[i-1] = muon.trackDist(i-4,2);
	else stations_w_track_at_boundary[i-1] = 0.;
      }
      if( muon.segmentX(i-4,2) < 999999 ) { //current "raw" info that a segment is matched to the current track
	++nr_of_stations_with_segment;
	station_has_segmentmatch[i-1] = 1;
      }
    }
    // rough estimation of chamber border efficiency (should be parametrized better, this is just a quick guess):
    // TF1 * merf = new TF1("merf","-0.5*(TMath::Erf(x/6.)-1)",-100,100);
    // use above value to "unpunish" missing segment if close to border, i.e. rather than not adding any weight, add
    // the one from the function. Only for dist ~> -10 cm, else full punish!.

    // ********************************************************;
    // *** fill local info for this muon (do some counting) ***;
    // ************** end *************************************;
  }

  // ********************************************************;
  // *** calculate weights for each station *****************;
  // ************** begin ***********************************;
  //    const float slope = 0.5;
  //    const float attenuate_weight_regain = 1.;
  // if attenuate_weight_regain < 1., additional punishment if track is close to boundary and no segment
  const float attenuate_weight_regain = 0.5; 

  for(int i = 1; i<=8; ++i) { // loop over all possible stations

    // first set all weights if a station has been crossed
    // later penalize if a station did not have a matching segment

    //old logic      if(station_has_segmentmatch[i-1] > 0 ) { // the track has an associated segment at the current station
    if( station_was_crossed[i-1] > 0 ) { // the track crossed this chamber (or was nearby)
      // - Apply a weight depending on the "depth" of the muon passage. 
      // - The station_weight is later reduced for stations with badly matched segments. 
      // - Even if there is no segment but the track passes close to a chamber boundary, the
      //   weight is set non zero and can go up to 0.5 of the full weight if the track is quite
      //   far from any station.
      ++position_in_stations;

      switch ( nr_of_stations_crossed ) { // define different weights depending on how many stations were crossed
      case 1 : 
	station_weight[i-1] =  1.;
	break;
      case 2 :
	if     ( position_in_stations == 1 ) station_weight[i-1] =  0.33;
	else                                 station_weight[i-1] =  0.67;
	break;
      case 3 : 
	if     ( position_in_stations == 1 ) station_weight[i-1] =  0.23;
	else if( position_in_stations == 2 ) station_weight[i-1] =  0.33;
	else                                 station_weight[i-1] =  0.44;
	break;
      case 4 : 
	if     ( position_in_stations == 1 ) station_weight[i-1] =  0.10;
	else if( position_in_stations == 2 ) station_weight[i-1] =  0.20;
	else if( position_in_stations == 3 ) station_weight[i-1] =  0.30;
	else                                 station_weight[i-1] =  0.40;
	break;
	  
      default : 
// 	LogTrace("MuonIdentification")<<"            // Message: A muon candidate track has more than 4 stations with matching segments.";
// 	LogTrace("MuonIdentification")<<"            // Did not expect this - please let me know: ibloch@fnal.gov";
	// for all other cases
	station_weight[i-1] = 1./nr_of_stations_crossed;
      }

      if( use_weight_regain_at_chamber_boundary ) { // reconstitute some weight if there is no match but the segment is close to a boundary:
	if(station_has_segmentmatch[i-1] <= 0 && stations_w_track_at_boundary[i-1] != 0. ) {
	  // if segment is not present but track in inefficient region, do not count as "missing match" but add some reduced weight. 
	  // original "match weight" is currently reduced by at least attenuate_weight_regain, variing with an error function down to 0 if the track is 
	  // inside the chamber.
	  station_weight[i-1] = station_weight[i-1]*attenuate_weight_regain*0.5*(TMath::Erf(stations_w_track_at_boundary[i-1]/6.)+1.); // remark: the additional scale of 0.5 normalizes Err to run from 0 to 1 in y
	}
	else if(station_has_segmentmatch[i-1] <= 0 && stations_w_track_at_boundary[i-1] == 0.) { // no segment match and track well inside chamber
	  // full penalization
	  station_weight[i-1] = 0.;
	}
      }
      else { // always fully penalize tracks with no matching segment, whether the segment is close to the boundary or not.
	if(station_has_segmentmatch[i-1] <= 0) station_weight[i-1] = 0.;
      }

      if( station_has_segmentmatch[i-1] > 0 && 42 == 42 ) { // if track has matching segment, but the matching is not high quality, penalize
	if(i<=4) { // we are in the DTs
	  if( muon.dY(i,1) < 999999 && muon.dX(i,1) < 999999) { // have both X and Y match
	    if(
	       TMath::Sqrt(TMath::Power(muon.pullX(i,1),2.)+TMath::Power(muon.pullY(i,1),2.))> 1. ) {
	      // reduce weight
	      if(use_match_dist_penalty) {
		// only use pull if 3 sigma is not smaller than 3 cm
		if(TMath::Sqrt(TMath::Power(muon.dX(i,1),2.)+TMath::Power(muon.dY(i,1),2.)) < 3. && TMath::Sqrt(TMath::Power(muon.pullX(i,1),2.)+TMath::Power(muon.pullY(i,1),2.)) > 3. ) { 
		  station_weight[i-1] *= 1./TMath::Power(
							 TMath::Max((double)TMath::Sqrt(TMath::Power(muon.dX(i,1),2.)+TMath::Power(muon.dY(i,1),2.)),(double)1.),.25); 
		}
		else {
		  station_weight[i-1] *= 1./TMath::Power(
							 TMath::Sqrt(TMath::Power(muon.pullX(i,1),2.)+TMath::Power(muon.pullY(i,1),2.)),.25); 
		}
	      }
	    }
	  }
	  else if (muon.dY(i,1) >= 999999) { // has no match in Y
	    if( muon.pullX(i,1) > 1. ) { // has a match in X. Pull larger that 1 to avoid increasing the weight (just penalize, don't anti-penalize)
	      // reduce weight
	      if(use_match_dist_penalty) {
		// only use pull if 3 sigma is not smaller than 3 cm
		if( muon.dX(i,1) < 3. && muon.pullX(i,1) > 3. ) { 
		  station_weight[i-1] *= 1./TMath::Power(TMath::Max((double)muon.dX(i,1),(double)1.),.25);
		}
		else {
		  station_weight[i-1] *= 1./TMath::Power(muon.pullX(i,1),.25);
		}
	      }
	    }
	  }
	  else { // has no match in X
	    if( muon.pullY(i,1) > 1. ) { // has a match in Y. Pull larger that 1 to avoid increasing the weight (just penalize, don't anti-penalize)
	      // reduce weight
	      if(use_match_dist_penalty) {
		// only use pull if 3 sigma is not smaller than 3 cm
		if( muon.dY(i,1) < 3. && muon.pullY(i,1) > 3. ) { 
		  station_weight[i-1] *= 1./TMath::Power(TMath::Max((double)muon.dY(i,1),(double)1.),.25);
		}
		else {
		  station_weight[i-1] *= 1./TMath::Power(muon.pullY(i,1),.25);
		}
	      }
	    }
	  }
	}
	else { // We are in the CSCs
	  if(
	     TMath::Sqrt(TMath::Power(muon.pullX(i-4,2),2.)+TMath::Power(muon.pullY(i-4,2),2.)) > 1. ) {
	    // reduce weight
	    if(use_match_dist_penalty) {
	      // only use pull if 3 sigma is not smaller than 3 cm
	      if(TMath::Sqrt(TMath::Power(muon.dX(i-4,2),2.)+TMath::Power(muon.dY(i-4,2),2.)) < 3. && TMath::Sqrt(TMath::Power(muon.pullX(i-4,2),2.)+TMath::Power(muon.pullY(i-4,2),2.)) > 3. ) { 
		station_weight[i-1] *= 1./TMath::Power(
						       TMath::Max((double)TMath::Sqrt(TMath::Power(muon.dX(i-4,2),2.)+TMath::Power(muon.dY(i-4,2),2.)),(double)1.),.25);
	      }
	      else {
		station_weight[i-1] *= 1./TMath::Power(
						       TMath::Sqrt(TMath::Power(muon.pullX(i-4,2),2.)+TMath::Power(muon.pullY(i-4,2),2.)),.25);
	      }
	    }
	  }
	}
      }
	
      // Thoughts:
      // - should penalize if the segment has only x OR y info
      // - should also use the segment direction, as it now works!
	
    }
    else { // track did not pass a chamber in this station - just reset weight
      station_weight[i-1] = 0.;
    }
      
    //increment final weight for muon:
    full_weight += station_weight[i-1];
  }

  // if we don't expect any matches, we set the compatibility to
  // 0.5 as the track is as compatible with a muon as it is with
  // background - we should maybe rather set it to -0.5!
  if( nr_of_stations_crossed == 0 ) {
    //      full_weight = attenuate_weight_regain*0.5;
    full_weight = 0.5;
  }

  // ********************************************************;
  // *** calculate weights for each station *****************;
  // ************** end *************************************;

  return full_weight;

}

bool muon::isGoodMuon( const reco::Muon& muon, 
			 AlgorithmType type,
			 double minCompatibility ) {
  if (!muon.isMatchesValid()) return false;
  bool goodMuon = false;
  
  switch( type ) {
  case TM2DCompatibility:
    // Simplistic first cut in the 2D segment- vs calo-compatibility plane. Will have to be refined!
    if( ( (0.8*caloCompatibility( muon ))+(1.2*segmentCompatibility( muon )) ) > minCompatibility ) goodMuon = true;
    else goodMuon = false;
    return goodMuon;
    break;
  default : 
    // 	LogTrace("MuonIdentification")<<"            // Invalid Algorithm Type called!";
    goodMuon = false;
    return goodMuon;
    break;
  }
}

bool muon::isGoodMuon( const reco::Muon& muon,
			 AlgorithmType type,
			 int minNumberOfMatches,
			 double maxAbsDx,
			 double maxAbsPullX,
			 double maxAbsDy,
			 double maxAbsPullY,
			 double maxChamberDist,
			 double maxChamberDistPull,
			 reco::Muon::ArbitrationType arbitrationType )
{
   if (!muon.isMatchesValid()) return false;
   bool goodMuon = false;

   if (type == TMLastStation) {
      // To satisfy my own paranoia, if the user specifies that the
      // minimum number of matches is zero, then return true.
      if(minNumberOfMatches == 0) return true;

      unsigned int theStationMask = muon.stationMask(arbitrationType);
      unsigned int theRequiredStationMask = RequiredStationMask(muon, maxChamberDist, maxChamberDistPull, arbitrationType);

      // Require that there be at least a minimum number of segments
      int numSegs = 0;
      int numRequiredStations = 0;
      for(int it = 0; it < 8; ++it) {
         if(theStationMask & 1<<it) ++numSegs;
         if(theRequiredStationMask & 1<<it) ++numRequiredStations;
      }

      // Make sure the minimum number of matches is not greater than
      // the number of required stations but still greater than zero
      // Note that we only do this in the barrel region!
      if (fabs(muon.eta()) < 1.2) {
         if(minNumberOfMatches > numRequiredStations)
            minNumberOfMatches = numRequiredStations;
         if(minNumberOfMatches < 1)
            minNumberOfMatches = 1;
      }

      if(numSegs >= minNumberOfMatches) goodMuon = 1;

      // Require that last required station have segment
      // If there are zero required stations keep track
      // of the last station with a segment so that we may
      // apply the quality cuts below to it instead
      int lastSegBit = 0;
      if(theRequiredStationMask) {
         for(int stationIdx = 7; stationIdx >= 0; --stationIdx)
            if(theRequiredStationMask & 1<<stationIdx)
               if(theStationMask & 1<<stationIdx) {
                  lastSegBit = stationIdx;
                  goodMuon &= 1;
                  break;
               } else {
                  goodMuon = false;
                  break;
               }
      } else {
         for(int stationIdx = 7; stationIdx >= 0; --stationIdx)
            if(theStationMask & 1<<stationIdx) {
               lastSegBit = stationIdx;
               break;
            }
      }

      if(!goodMuon) return false;

      // Impose pull cuts on last segment
      int station = 0, detector = 0;
      station  = lastSegBit < 4 ? lastSegBit+1 : lastSegBit-3;
      detector = lastSegBit < 4 ? 1 : 2;

      // Check x information
      if(fabs(muon.pullX(station,detector,arbitrationType,1)) > maxAbsPullX &&
            fabs(muon.dX(station,detector,arbitrationType)) > maxAbsDx)
         return false;

      // Is this a tight algorithm, i.e. do we bother to check y information?
      if (maxAbsDy < 999999) { // really if maxAbsDy < 1E9 as currently defined

         // Check y information
         if (detector == 2) { // CSC
            if(fabs(muon.pullY(station,2,arbitrationType,1)) > maxAbsPullY &&
                  fabs(muon.dY(station,2,arbitrationType)) > maxAbsDy)
               return false;
         } else {
            //
            // In DT, if this is a "Tight" algorithm and the last segment is
            // missing y information (always the case in station 4!!!), impose
            // respective cuts on the next station in the stationMask that has
            // a segment with y information.  If there are no segments with y
            // information then there is nothing to penalize. Should we
            // penalize in Tight for having zero segments with y information?
            // That is the fundamental question.  Of course I am being uber
            // paranoid; if this is a good muon then there will probably be at
            // least one segment with y information but not always.  Suppose
            // somehow a muon only creates segments in station 4, then we
            // definitely do not want to require that there be at least one
            // segment with y information because we will lose it completely.
            //

            for (int stationIdx = station; stationIdx > 0; --stationIdx) {
               if(! (theStationMask & 1<<(stationIdx-1)))  // don't bother if the station is not in the stationMask
                  continue;

               if(muon.dY(stationIdx,1,arbitrationType) > 999998) // no y-information
                  continue;

               if(fabs(muon.pullY(stationIdx,1,arbitrationType,1)) > maxAbsPullY &&
                     fabs(muon.dY(stationIdx,1,arbitrationType)) > maxAbsDy) {
                  return false;
               }

               // If we get this far then great this is a good muon
               return true;
            }
         }
      }

      return goodMuon;
   } // TMLastStation

   // TMOneStation requires only that there be one "good" segment, regardless
   // of the required stations.  We do not penalize if there are absolutely zero
   // segments with y information in the Tight algorithm.  Maybe I'm being
   // paranoid but so be it.  If it's really a good muon then we will probably
   // find at least one segment with both x and y information but you never
   // know, and I don't want to deal with a potential inefficiency in the DT
   // like we did with the original TMLastStation.  Incidentally, not penalizing
   // for total lack of y information in the Tight algorithm is what is done in
   // the new TMLastStation
   //                   
   if (type == TMOneStation) {
      unsigned int theStationMask = muon.stationMask(arbitrationType);

      // Of course there must be at least one segment
      if (! theStationMask) return false;

      int  station = 0, detector = 0;
      // Keep track of whether or not there is a DT segment with y information.
      // In the end, if it turns out there are absolutely zero DT segments with
      // y information, require only that there was a segment with good x info.
      // This of course only applies to the Tight algorithms.
      bool existsGoodDTSegX = false;
      bool existsDTSegY = false;

      // Impose cuts on the segments in the station mask until we find a good one
      // Might as well start with the lowest bit to speed things up.
      for(int stationIdx = 0; stationIdx <= 7; ++stationIdx)
         if(theStationMask & 1<<stationIdx) {
            station  = stationIdx < 4 ? stationIdx+1 : stationIdx-3;
            detector = stationIdx < 4 ? 1 : 2;

            if(fabs(muon.pullX(station,detector,arbitrationType,1)) > maxAbsPullX &&
                  fabs(muon.dX(station,detector,arbitrationType)) > maxAbsDx)
               continue;
            else if (detector == 1)
               existsGoodDTSegX = true;

            // Is this a tight algorithm?  If yes, use y information
            if (maxAbsDy < 999999) {
               if (detector == 2) { // CSC
                  if(fabs(muon.pullY(station,2,arbitrationType,1)) > maxAbsPullY &&
                        fabs(muon.dY(station,2,arbitrationType)) > maxAbsDy)
                     continue;
               } else {

                  if(muon.dY(station,1,arbitrationType) > 999998) // no y-information
                     continue;
                  else
                     existsDTSegY = true;

                  if(fabs(muon.pullY(station,1,arbitrationType,1)) > maxAbsPullY &&
                        fabs(muon.dY(station,1,arbitrationType)) > maxAbsDy) {
                     continue;
                  }
               }
            }

            // If we get this far then great this is a good muon
            return true;
         }

      // If we get this far then for sure there are no "good" CSC segments. For
      // DT, check if there were any segments with y information.  If there
      // were none, but there was a segment with good x, then we're happy. If 
      // there WERE segments with y information, then they must have been shit
      // since we are here so fail it.  Of course, if this is a Loose algorithm
      // then fail immediately since if we had good x we would already have
      // returned true
      if (maxAbsDy < 999999) {
         if (existsDTSegY)
            return false;
         else if (existsGoodDTSegX)
            return true;
      } else
         return false;
   } // TMOneStation

   return goodMuon;
}

bool muon::isGoodMuon( const reco::Muon& muon, SelectionType type )
{
  switch (type)
     {
      case muon::All:
	return true;
	break;
      case muon::AllGlobalMuons:
	return muon.isGlobalMuon();
	break;
      case muon::AllTrackerMuons:
	return muon.isTrackerMuon();
	break;
      case muon::AllStandAloneMuons:
	return muon.isStandAloneMuon();
	break;
      case muon::TrackerMuonArbitrated:
	return muon.isTrackerMuon() && muon.numberOfMatches(reco::Muon::SegmentAndTrackArbitration)>0;
	break;
      case muon::AllArbitrated:
	return ! muon.isTrackerMuon() || muon.numberOfMatches(reco::Muon::SegmentAndTrackArbitration)>0;
	break;
      case muon::GlobalMuonPromptTight:
	return muon.isGlobalMuon() && muon.globalTrack()->normalizedChi2()<10. && muon.globalTrack()->hitPattern().numberOfValidMuonHits() >0;
	break;
   // For "Loose" algorithms we choose maximum y quantity cuts of 1E9 instead of
   // 9999 as before.  We do this because the muon methods return 999999 (note
   // there are six 9's) when the requested information is not available.  For
   // example, if a muon fails to traverse the z measuring superlayer in a station
   // in the DT, then all methods involving segmentY in this station return
   // 999999 to demonstrate that the information is missing.  In order to not
   // penalize muons for missing y information in Loose algorithms where we do
   // not care at all about y information, we raise these limits.  In the
   // TMLastStation and TMOneStation algorithms we actually use this huge number
   // to determine whether to consider y information at all.
      case muon::TMLastStationLoose:
	return isGoodMuon(muon,TMLastStation,2,3,3,1E9,1E9,-3,-3,reco::Muon::SegmentAndTrackArbitration);
	break;
      case muon::TMLastStationTight:
	return isGoodMuon(muon,TMLastStation,2,3,3,3,3,-3,-3,reco::Muon::SegmentAndTrackArbitration);
	break;
      case muon::TMOneStationLoose:
	return isGoodMuon(muon,TMOneStation,1,3,3,1E9,1E9,1E9,1E9,reco::Muon::SegmentAndTrackArbitration);
	break;
      case muon::TMOneStationTight:
	return isGoodMuon(muon,TMOneStation,1,3,3,3,3,1E9,1E9,reco::Muon::SegmentAndTrackArbitration);
	break;
      case muon::TMLastStationOptimizedLowPtLoose:
   if (muon.pt() < 8. && fabs(muon.eta()) < 1.2)
	   return isGoodMuon(muon,TMOneStation,1,3,3,1E9,1E9,1E9,1E9,reco::Muon::SegmentAndTrackArbitration);
   else
	   return isGoodMuon(muon,TMLastStation,2,3,3,1E9,1E9,-3,-3,reco::Muon::SegmentAndTrackArbitration);
	break;
      case muon::TMLastStationOptimizedLowPtTight:
   if (muon.pt() < 8. && fabs(muon.eta()) < 1.2)
	   return isGoodMuon(muon,TMOneStation,1,3,3,3,3,1E9,1E9,reco::Muon::SegmentAndTrackArbitration);
   else
	   return isGoodMuon(muon,TMLastStation,2,3,3,3,3,-3,-3,reco::Muon::SegmentAndTrackArbitration);
	break;
	//compatibility loose
      case muon::TM2DCompatibilityLoose:
	return isGoodMuon(muon,TM2DCompatibility,0.7);
	break;
	//compatibility tight
      case muon::TM2DCompatibilityTight:
	return isGoodMuon(muon,TM2DCompatibility,1.0);
	break;
      default:
	return false;
     }
}
