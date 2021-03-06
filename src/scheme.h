#pragma once

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace types {
struct Scheme;
typedef std::shared_ptr<const Scheme> SchemeRef;
typedef std::map<std::string, SchemeRef> SchemeMap;
typedef std::set<SchemeRef> SchemeSet;
typedef std::list<SchemeRef> Schemes;
} // namespace types

#include "class_predicate.h"
#include "location.h"
#include "types.h"

namespace types {

struct Scheme final : public std::enable_shared_from_this<Scheme> {
  typedef std::shared_ptr<const Scheme> Ref;
  typedef std::vector<Ref> Refs;
  typedef std::map<std::string, Ref> Map;

  Scheme(Location location,
         const std::vector<std::string> &vars,
         const ClassPredicates &predicates,
         types::Ref type);

  types::Ref instantiate(Location location) const;
  Scheme::Ref rebind(const types::Map &env) const;
  Scheme::Ref normalize() const;

  Scheme::Ref freshen(Location location) const;
  Scheme::Ref freshen() const;

  /* count of the constrained type variables */
  int btvs() const;
  Ftvs ftvs() const;

  std::string str() const;
  std::string repr() const;
  Location get_location() const;

  Location location;
  std::vector<std::string> const vars;
  ClassPredicates const predicates;
  types::Ref const type;

private:
  mutable bool has_ftvs = false;
  mutable Ftvs cached_ftvs;
};

} // namespace types

types::SchemeRef scheme(Location location,
                        std::vector<std::string> vars,
                        const types::ClassPredicates &predicates,
                        const types::Ref &type);

std::string str(const types::Scheme::Map &m);
