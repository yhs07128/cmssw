/**  \class GlobalMuonReFitter
 *
 *   Algorithm to refit a muon track in the
 *   muon chambers and the tracker.
 *   It consists of a standard Kalman forward fit
 *   and a Kalman backward smoother.
 *
 *
 *   $Date: 2006/07/21 19:16:41 $
 *   $Revision: 1.4 $
 *
 *   \author   N. Neumeister            Purdue University
 *   \author   I. Belotelov             DUBNA
 *    \porting author C. Liu            Purdue University
 **/

#include "RecoMuon/GlobalTrackFinder/interface/GlobalMuonReFitter.h"

#include <iostream>
                                                                                

#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "TrackingTools/GeomPropagators/interface/Propagator.h"
#include "TrackingTools/Records/interface/TrackingComponentsRecord.h"
#include "TrackingTools/KalmanUpdators/interface/KFUpdator.h"
#include "TrackingTools/KalmanUpdators/interface/Chi2MeasurementEstimator.h"
#include "TrackingTools/TrackFitters/interface/TrajectoryStateWithArbitraryError.h"
#include "TrackingTools/TrackFitters/interface/TrajectoryStateCombiner.h"
#include "TrackingTools/TransientTrackingRecHit/interface/TransientTrackingRecHit.h"
#include "TrackingTools/GeomPropagators/interface/SmartPropagator.h"
#include "Geometry/CommonDetUnit/interface/GeomDet.h"

#include "MagneticField/Engine/interface/MagneticField.h"
#include "MagneticField/Records/interface/IdealMagneticFieldRecord.h"
#include "RecoMuon/TrackingTools/interface/MuonTrajectoryUpdator.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"

using namespace edm;
using namespace std;

///constructor
GlobalMuonReFitter::GlobalMuonReFitter(const ParameterSet& par) {

  theErrorRescaling = 100.0;
 
  theUpdator     = new KFUpdator;
  theEstimator   = new Chi2MeasurementEstimator(20000.0);
  theInPropagatorAlongMom = par.getParameter<string>("InPropagatorAlongMom");
  theOutPropagatorAlongMom = par.getParameter<string>("OutPropagatorAlongMom");
  theInPropagatorOppositeToMom = par.getParameter<string>("InPropagatorOppositeToMom");
  theOutPropagatorOppositeToMom = par.getParameter<string>("OutPropagatorOppositeToMom");


}

///destructor
GlobalMuonReFitter::~GlobalMuonReFitter() {
  if (thePropagator2) delete thePropagator2;
  if (thePropagator1) delete thePropagator1;
  if (theUpdator) delete theUpdator;
  if (theEstimator) delete theEstimator;

}

void GlobalMuonReFitter::setES(const edm::EventSetup& iSetup) {

  ESHandle<Propagator> eshPropagator1;
  iSetup.get<TrackingComponentsRecord>().get(theOutPropagatorAlongMom, eshPropagator1);

  ESHandle<Propagator> eshPropagator2;
  iSetup.get<TrackingComponentsRecord>().get(theInPropagatorAlongMom, eshPropagator2);

  ESHandle<Propagator> eshPropagator3;
  iSetup.get<TrackingComponentsRecord>().get(theOutPropagatorOppositeToMom, eshPropagator3);

  ESHandle<Propagator> eshPropagator4;
  iSetup.get<TrackingComponentsRecord>().get(theInPropagatorOppositeToMom, eshPropagator4);

  iSetup.get<IdealMagneticFieldRecord>().get(theField);

  thePropagator1 = new SmartPropagator(*eshPropagator2,*eshPropagator1, &*theField);
  thePropagator2 = new SmartPropagator(*eshPropagator4,*eshPropagator3, &*theField,oppositeToMomentum);

  theTrajectoryUpdator = new MuonTrajectoryUpdator(thePropagator1, 1000.0, 0);

}


vector<Trajectory> GlobalMuonReFitter::trajectories(const Trajectory& t) const {
  if ( !t.isValid() ) return vector<Trajectory>();
  vector<Trajectory> fitted = fit(t);
  return smooth(fitted);

}


vector<Trajectory> GlobalMuonReFitter::trajectories(const TrajectorySeed& seed,
	                                      const edm::OwnVector<const TransientTrackingRecHit>& hits, 
	                                      const TrajectoryStateOnSurface& firstPredTsos) const {

  if ( hits.empty() ) return vector<Trajectory>();

  TSOS firstTsos = TrajectoryStateWithArbitraryError()(firstPredTsos);
  vector<Trajectory> fitted = fit(seed, hits, firstTsos);

  return smooth(fitted);

}

vector<Trajectory> GlobalMuonReFitter::fit(const Trajectory& t) const {

  if ( t.empty() ) return vector<Trajectory>();

  TM firstTM = t.firstMeasurement();
  TSOS firstTsos = TrajectoryStateWithArbitraryError()(firstTM.updatedState());
  return fit(t.seed(), t.recHits(), firstTsos);

}


vector<Trajectory> GlobalMuonReFitter::fit(const TrajectorySeed& seed,
			             const edm::OwnVector<const TransientTrackingRecHit>& hits, 
				     const TSOS& firstPredTsos) const {

  if ( hits.empty() ) return vector<Trajectory>();
  Trajectory myTraj(seed, thePropagator1->propagationDirection());

  TSOS predTsos(firstPredTsos);
  if ( !predTsos.isValid() ) return vector<Trajectory>();
 
  for ( edm::OwnVector<const TransientTrackingRecHit>::const_iterator ihit = hits.begin(); 
        ihit != hits.end(); ++ihit ) {


    const TM* theMeas = new TM(predTsos,(&*ihit)); 
      // update
    pair<bool,TrajectoryStateOnSurface> result = theTrajectoryUpdator->update(theMeas,myTraj);
      if(result.first){ 
	predTsos = result.second;
      }else predTsos = thePropagator1->propagate(predTsos, (*ihit).det()->surface());

  }
  if ( !myTraj.isValid() ) return vector<Trajectory>();

  return vector<Trajectory>(1, myTraj);
}


vector<Trajectory> GlobalMuonReFitter::smooth(const vector<Trajectory>& tc) const {

  vector<Trajectory> result; 

  for ( vector<Trajectory>::const_iterator it = tc.begin(); it != tc.end(); ++it ) {
    vector<Trajectory> smoothed = smooth(*it);
    result.insert(result.end(), smoothed.begin(), smoothed.end());
  }

  return result;

}


vector<Trajectory> GlobalMuonReFitter::smooth(const Trajectory& t) const {

  if ( t.empty() ) return vector<Trajectory>();

  Trajectory myTraj(t.seed(), thePropagator2->propagationDirection());
  vector<TM> avtm = t.measurements();

  if ( avtm.size() < 2 ) return vector<Trajectory>(); 

  TSOS predTsos = avtm.back().forwardPredictedState();
  predTsos.rescaleError(theErrorRescaling);
  if ( !predTsos.isValid() ) return vector<Trajectory>();
  TSOS currTsos;

  // first smoothed TM is last fitted
  if ( avtm.back().recHit()->isValid() ) {

    currTsos = theUpdator->update(predTsos, (*avtm.back().recHit()));

    myTraj.push(TM(avtm.back().forwardPredictedState(), 
		   predTsos,
		   avtm.back().updatedState(), 
		   avtm.back().recHit(),
		   avtm.back().estimate(),
		   avtm.back().layer()), 
		avtm.back().estimate());

  } else {

    currTsos = predTsos;

    myTraj.push(TM(avtm.back().forwardPredictedState(),
		   avtm.back().recHit(),
                   avtm.back().estimate(),
		   avtm.back().layer()));

  }

  TrajectoryStateCombiner combiner;

  for ( vector<TM>::reverse_iterator itm = avtm.rbegin() + 1; 
        itm != avtm.rend() - 1; ++itm ) {
    predTsos = thePropagator2->propagate(currTsos,(*itm).recHit()->det()->surface());

    if ( !predTsos.isValid() ) {
      return vector<Trajectory>();
    } else if ( (*itm).recHit()->isValid() ) {
      //update
      currTsos = theUpdator->update(predTsos, (*(*itm).recHit()));
      TSOS combTsos = combiner(predTsos, (*itm).forwardPredictedState());
      if ( !combTsos.isValid() ) return vector<Trajectory>();

      TSOS smooTsos = combiner((*itm).updatedState(), predTsos);

      if ( !smooTsos.isValid() ) return vector<Trajectory>();
      myTraj.push(TM((*itm).forwardPredictedState(),
		     predTsos,
		     smooTsos,
		     (*itm).recHit(),
		     theEstimator->estimate(combTsos, (*(*itm).recHit())).second,
		     (*itm).layer()),
		  (*itm).estimate());
    } else {
      currTsos = predTsos;
      TSOS combTsos = combiner(predTsos, (*itm).forwardPredictedState());
      
      if ( !combTsos.isValid() ) return vector<Trajectory>();
      myTraj.push(TM((*itm).forwardPredictedState(),
		     predTsos,
		     combTsos,
		     (*itm).recHit(),
                     (*itm).estimate(),
		     (*itm).layer()));
    }
  }

  // last smoothed TM is last filtered
  predTsos = thePropagator2->propagate(currTsos, avtm.front().recHit()->det()->surface());
  
  if ( !predTsos.isValid() ) return vector<Trajectory>();
  if ( avtm.front().recHit()->isValid() ) {
    //update
    currTsos = theUpdator->update(predTsos, (*avtm.front().recHit()));
    myTraj.push(TM(avtm.front().forwardPredictedState(),
		   predTsos,
		   currTsos,
		   avtm.front().recHit(),
		   theEstimator->estimate(predTsos, (*avtm.front().recHit())).second,
		   avtm.front().layer()),
		avtm.front().estimate());
  } else {
    myTraj.push(TM(avtm.front().forwardPredictedState(),
		   avtm.front().recHit(),
                   avtm.front().estimate(),
		   avtm.front().layer()));
  }

  return vector<Trajectory>(1, myTraj); 

}


