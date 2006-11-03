#ifndef Framework_FilterWorker_h
#define Framework_FilterWorker_h

/*----------------------------------------------------------------------
  
FilterWorker: The EDFilter as the schedule sees it.  The job of
this object is to call the filter.
According to our current definition, a single filter can only
appear in one worker.

$Id: FilterWorker.h,v 1.16 2006/09/01 18:16:42 wmtan Exp $

----------------------------------------------------------------------*/

#include <memory>

#include "boost/shared_ptr.hpp"

#include "FWCore/Framework/src/Worker.h"
#include "FWCore/Framework/interface/Frameworkfwd.h"

namespace edm
{

  class FilterWorker : public Worker
  {
  public:
    FilterWorker(std::auto_ptr<EDFilter>,
		 const ModuleDescription&,
		 const WorkerParams&);
    virtual ~FilterWorker();

    template <class ModType>
    static std::auto_ptr<EDFilter> makeOne(const ModuleDescription&,
					   const WorkerParams&);

  private:
    virtual bool implDoWork(EventPrincipal& e, EventSetup const& c,
			    CurrentProcessingContext const* cpc);
    virtual void implBeginJob(EventSetup const&) ;
    virtual void implEndJob() ;
    virtual bool implBeginRun(RunPrincipal& rp, EventSetup const& c,
			    CurrentProcessingContext const* cpc);
    virtual bool implEndRun(RunPrincipal& rp, EventSetup const& c,
			    CurrentProcessingContext const* cpc);
    virtual bool implBeginLuminosityBlock(LuminosityBlockPrincipal& lbp, EventSetup const& c,
			    CurrentProcessingContext const* cpc);
    virtual bool implEndLuminosityBlock(LuminosityBlockPrincipal& lbp, EventSetup const& c,
			    CurrentProcessingContext const* cpc);
    virtual std::string workerType() const;

    boost::shared_ptr<EDFilter> filter_;
  };

  template <> 
  struct WorkerType<EDFilter>
  {
    typedef EDFilter ModuleType;
    typedef FilterWorker worker_type;
  };

  template <class ModType>
  std::auto_ptr<EDFilter> FilterWorker::makeOne(const ModuleDescription&,
						const WorkerParams& wp)
  {
    return std::auto_ptr<EDFilter>(new ModType(*wp.pset_));
  }

}

#endif
