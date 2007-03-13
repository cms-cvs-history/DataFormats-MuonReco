#ifndef MuonReco_Muon_h
#define MuonReco_Muon_h
/** \class reco::Muon Muon.h DataFormats/MuonReco/interface/Muon.h
 *  
 * A reconstructed Muon.
 * contains reference to three fits:
 *  - tracker alone
 *  - muon detector alone
 *  - combined muon plus tracker
 *
 * \author Luca Lista, INFN
 *
 * \version $Id: Muon.h,v 1.25 2007/03/13 08:59:19 llista Exp $
 *
 */
#include "DataFormats/RecoCandidate/interface/RecoCandidate.h"

namespace reco {
 
  class Muon : public RecoCandidate {
  public:
    Muon() { }
    /// constructor from values
    Muon(  Charge, const LorentzVector &, const Point & = Point( 0, 0, 0 ) );
    /// reference to Track reconstructed in the tracker only
    TrackRef track() const { return track_; }
    /// reference to Track reconstructed in the muon detector only
    TrackRef standAloneMuon() const { return standAloneMuon_; }
    /// reference to Track reconstructed in both tracked and muon detector
    TrackRef combinedMuon() const { return combinedMuon_; }
    /// set reference to Track
    void setTrack( const TrackRef & t ) { track_ = t; }
    /// set reference to Track
    void setStandAlone( const TrackRef & t ) { standAloneMuon_ = t; }
    /// set reference to Track
    void setCombined( const TrackRef & t ) { combinedMuon_ = t; }

  private:
    /// check overlap with another candidate
    virtual bool overlap( const Candidate & ) const;
    /// reference to Track reconstructed in the tracker only
    TrackRef track_;
    /// reference to Track reconstructed in the muon detector only
    TrackRef standAloneMuon_;
    /// reference to Track reconstructed in both tracked and muon detector
    TrackRef combinedMuon_;
  };

}

#include "DataFormats/MuonReco/interface/MuonFwd.h"

#endif
