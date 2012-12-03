#ifndef MuonReco_MuonCocktails_h
#define MuonReco_MuonCocktails_h

/** \class MuonCocktails
 *
 *  Set of functions that select among the different track refits
 *  based on the fit quality, in order to achieve optimal resolution.
 *
 *  $Date: 2011/09/21 01:02:50 $
 *  $Revision: 1.2 $
 *  \author Piotr Traczyk
 */

#include "DataFormats/MuonReco/interface/Muon.h"
#include "DataFormats/TrackReco/interface/TrackFwd.h"
#include "DataFormats/TrackReco/interface/TrackToTrackMap.h"

namespace muon {

  reco::TrackRef tevOptimized(const reco::TrackRef& combinedTrack,
			      const reco::TrackRef& trackerTrack,
			      const reco::TrackRef& tpfmsTrack,
			      const reco::TrackRef& pickyTrack,
 			      const double ptThreshold = 200.,
			      const double tune1 = 30.,
			      const double tune2 = 0.,
			      double dptcut = -1.);

  // Version for convenience. (NB: can be used with pat::Muon, even
  // with embedded tracks, equally conveniently!)
  inline reco::TrackRef tevOptimized(const reco::Muon& muon,
 			      const double ptThreshold = 200.,
			      const double tune1 = 30.,
			      const double tune2 = 0.,
			      const double dptcut = -1.) {
    return tevOptimized(muon.globalTrack(),
			muon.innerTrack(),
			muon.tpfmsTrack(),
			muon.pickyTrack(),
			ptThreshold,
			tune1,
			tune2,
			dptcut);
  }

  // The next two versions of tevOptimized are for backward
  // compatibility; TrackToTrackMaps are to be removed from the
  // EventContent, so these versions will go away (along with the
  // helper getter function) after a deprecation period. Since they
  // are just for backward compatibility and not for new code, we
  // don't bother to expose the tune parameters.

  inline reco::TrackRef getTevRefitTrack(const reco::TrackRef& combinedTrack,
				  const reco::TrackToTrackMap& map) {
    reco::TrackToTrackMap::const_iterator it = map.find(combinedTrack);
    return it == map.end() ? reco::TrackRef() : it->val;
  }

  inline reco::TrackRef tevOptimized(const reco::TrackRef& combinedTrack,
			      const reco::TrackRef& trackerTrack,
			      const reco::TrackToTrackMap& tevMap1,
			      const reco::TrackToTrackMap& tevMap2,
			      const reco::TrackToTrackMap& tevMap3) {
    return tevOptimized(combinedTrack,
			trackerTrack,
			getTevRefitTrack(combinedTrack, tevMap2),
			getTevRefitTrack(combinedTrack, tevMap3));
  }

  inline reco::TrackRef tevOptimized(const reco::Muon& muon,
			      const reco::TrackToTrackMap& tevMap1,
			      const reco::TrackToTrackMap& tevMap2,
			      const reco::TrackToTrackMap& tevMap3 ) {
    return tevOptimized(muon.combinedMuon(),
			muon.track(),
			getTevRefitTrack(muon.combinedMuon(), tevMap2),
			getTevRefitTrack(muon.combinedMuon(), tevMap3));
  }

  // This old version of the cocktail function is deprecated and so it
  // doesn't get the new interface.
  reco::TrackRef tevOptimizedOld( const reco::TrackRef& combinedTrack,
				  const reco::TrackRef& trackerTrack,
				  const reco::TrackToTrackMap tevMap1,
				  const reco::TrackToTrackMap tevMap2,
				  const reco::TrackToTrackMap tevMap3 );
  
  inline reco::TrackRef tevOptimizedOld( const reco::Muon& muon,
				  const reco::TrackToTrackMap tevMap1,
				  const reco::TrackToTrackMap tevMap2,
				  const reco::TrackToTrackMap tevMap3 ) {
    return tevOptimizedOld(muon.combinedMuon(), muon.track(), tevMap1, tevMap2, tevMap3);
  }

  // The cocktail used as the soon-to-be-old default momentum
  // assignment for the reco::Muon.
  reco::TrackRef sigmaSwitch(const reco::TrackRef& combinedTrack,
			     const reco::TrackRef& trackerTrack,
			     const double nSigma = 2.,
			     const double ptThreshold = 200.);

  // Convenience version of the above.
  inline reco::TrackRef sigmaSwitch(const reco::Muon& muon,
			     const double nSigma = 2.,
                             const double ptThreshold = 200.) {
    return muon::sigmaSwitch(muon.globalTrack(),
			     muon.innerTrack(),
			     nSigma,
			     ptThreshold);
  }

  // "Truncated muon reconstructor": the first cocktail, between just
  // tracker-only and TPFMS. Similar to tevOptimized.
  reco::TrackRef TMR(const reco::TrackRef& trackerTrack,
		     const reco::TrackRef& fmsTrack,
		     const double tune=4.);

  double trackProbability(const reco::TrackRef track);
}

#endif
