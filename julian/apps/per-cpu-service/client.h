#pragma once

#include <nul/capalloc.h>
#include <nul/baseprogram.h>
#include <nul/parent.h>

/// A simple per-cpu client. Constructing a CpuLocalClient implicitly
/// creates a session to the given service. Sessions are not closed on
/// object destruction, as it might interfere with modularity (see
/// comments in ~CpuLocalClient). Subclasses of CpuLocalClient (and
/// services) are expected to add another layer to multiplex multiple
/// connections using the same pseudonym (i.e. running in the same
/// PD).
class CpuLocalClient {
protected:
  Utcb           &_utcb;
  CapAllocator   *_cap_alloc;
  const char     *_service;
  
  unsigned _session;
  unsigned _pseudonym;
  unsigned _portal;
  
public:

  unsigned call()
  {
    return nova_call(_session);
  }

  // Create session for the calling CPU.
  CpuLocalClient(Utcb &utcb, CapAllocator *cap, const char *service, unsigned instance, bool blocking = true)
    : _utcb(utcb), _cap_alloc(cap), _service(service)

  {
    unsigned res;
    _pseudonym = cap->alloc_cap();

    res = ParentProtocol::get_pseudonym(utcb, service, instance, _pseudonym);
    if (res != ENONE) Logging::panic("Couldn't get pseudonym for service '%s'.",
                                     service);
    _portal = cap->alloc_cap();

    res = ParentProtocol::get_portal(utcb, _pseudonym, _portal, blocking);
    if (res != ENONE) Logging::panic("Couldn't get service portal for service '%s'.",
                                     service);
    
    utcb.add_frame()
      << ParentProtocol::TYPE_OPEN << Utcb::TypedMapCap(_pseudonym)
      << Crd(_session, 0, DESC_CAP_ALL);
    res = ParentProtocol::call(utcb, _portal, true, false);
    if (res != ENONE) Logging::panic("Couldn't open session to '%s': %u",
                                     service, res);
  }


  ~CpuLocalClient() {
    // unsigned res;
    assert(BaseProgram::myutcb() == &_utcb);

    // Closing the session explicitly would break other sessions to
    // the same service (see below).
    // _utcb.add_frame()
    //   << ParentProtocol::TYPE_CLOSE << Utcb::TypedMapCap(_pseudonym);
    // res = ParentProtocol::call(_utcb, _portal, true, false);
    // assert(res == ENONE);

    // We cannot destroy our pseudonym, because this would break other
    // sessions using the same pseudonym (perhaps in libraries). 
    // res = ParentProtocol::release_pseudonym(_utcb, _pseudonym);
    // assert(res == ENONE);

    nova_revoke(Crd(_pseudonym, 0, DESC_CAP_ALL), true);
    nova_revoke(Crd(_portal, 0, DESC_CAP_ALL),    true);
    nova_revoke(Crd(_session, 0, DESC_CAP_ALL),   true);
    _cap_alloc->dealloc_cap(_pseudonym);
    _cap_alloc->dealloc_cap(_portal);
    _cap_alloc->dealloc_cap(_session);

  }
};

// EOF
