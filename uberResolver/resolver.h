#ifndef AR_UBER_RESOLVER_H
#define AR_UBER_RESOLVER_H

#include "pxr/usd/ar/defaultResolver.h"

#include <tbb/enumerable_thread_specific.h>

#include <memory>
#include <string>
#include <vector>

/// \class uberResolver
///
/// A generic resolver, which can load up resolver plugins and assign them to
/// predefined URIs.
///
class uberResolver
    : public ArDefaultResolver
{
public:
    uberResolver();
    virtual ~uberResolver();
};

#endif // AR_UBER_RESOLVER_H
