//
// $Source$
// $Date$
// $Revision$
//
// DESCRIPTION:
// Implementation of strategy classes for normal forms
//
// This file is part of Gambit
// Copyright (c) 2002, The Gambit Project
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//

#include "base/base.h"
#include "nfg.h"
#include "nfstrat.h"
#include "nfgint.h"
#include "efg.h"

//--------------------------------------------------------
// gbtNfgContingency: Constructors, Destructors, Operators
//--------------------------------------------------------

gbtNfgContingency::gbtNfgContingency(const gbtNfgGame &p_nfg)
  : m_nfg(p_nfg), index(0L), profile(m_nfg.NumPlayers())
{
  for (int pl = 1; pl <= m_nfg.NumPlayers(); pl++)   {
    profile[pl] = m_nfg.GetPlayer(pl).GetStrategy(1);
    index += profile[pl].GetIndex();
  }
}

gbtNfgContingency::gbtNfgContingency(const gbtNfgContingency &p)
  : m_nfg(p.m_nfg), index(p.index), profile(p.profile)
{ }

gbtNfgContingency::~gbtNfgContingency()
{ }

gbtNfgContingency &gbtNfgContingency::operator=(const gbtNfgContingency &p)
{
  if (this != &p) {
    m_nfg = p.m_nfg;
    index = p.index;
    profile = p.profile;
  }
  return *this;  
}

//-----------------------------------------
// gbtNfgContingency: Members
//-----------------------------------------

bool gbtNfgContingency::IsValid(void) const
{
  int i;
  for (i = profile.Length(); i > 0 && profile[i] != 0; i--);
  return i;
}

long gbtNfgContingency::GetIndex(void) const 
{ 
  return index; 
}

gbtNfgStrategy gbtNfgContingency::operator[](int p) const 
{ 
  return profile[p];
}

gbtNfgStrategy gbtNfgContingency::Get(int p) const 
{ 
  return profile[p];
}

void gbtNfgContingency::Set(int p, gbtNfgStrategy s)
{
  index += s.GetIndex() - profile[p].GetIndex();
  profile[p] = s;
}

void gbtNfgContingency::SetOutcome(const gbtNfgOutcome &outcome)
{
  m_nfg.rep->m_results[index + 1] = outcome.rep;
  m_nfg.rep->m_revision++;
  m_nfg.BreakLink();
}

gbtNfgOutcome gbtNfgContingency::GetOutcome(void) const
{
  return m_nfg.rep->m_results[index + 1];
}

gbtNumber gbtNfgContingency::GetPayoff(const gbtNfgPlayer &p_player) const
{
  if (m_nfg.rep->m_results.Length() > 0) {
    return m_nfg.rep->m_results[index + 1]->m_payoffs[p_player.GetId()];
  }
  else {
    gbtArray<gbtArray<int> *> behav(m_nfg.NumPlayers());
    for (int pl = 1; pl <= behav.Length(); pl++) {
      // Casting away const -- sloppy
      behav[pl] = (gbtArray<int> *) profile[pl].GetBehavior();
    }
    gbtVector<gbtNumber> payoff(m_nfg.NumPlayers());
    gbtEfgGame(m_nfg.rep->m_efg).Payoff(behav, payoff);
    return payoff[p_player.GetId()];
  }
}

//==========================================================================
//                         class gbtNfgSupport
//==========================================================================

//--------------------------------------------------------------------------
//                   class gbtNfgSupport: Lifecycle
//--------------------------------------------------------------------------

gbtNfgSupport::gbtNfgSupport(const gbtNfgGame &p_nfg)
  : m_nfg(p_nfg), m_strategies(p_nfg.NumStrats())
{ 
  // Initially, all strategies are contained in the support
  m_strategies = 1;
}

gbtNfgSupport &gbtNfgSupport::operator=(const gbtNfgSupport &p_support)
{
  if (this != &p_support && m_nfg == p_support.m_nfg) {
    m_label = p_support.m_label;
    m_strategies = p_support.m_strategies;
  }
  return *this;
}

//--------------------------------------------------------------------------
//                   class gbtNfgSupport: Operators
//--------------------------------------------------------------------------

bool gbtNfgSupport::operator==(const gbtNfgSupport &p_support) const
{
  return (m_nfg == p_support.m_nfg && m_strategies == p_support.m_strategies);
}
  
//--------------------------------------------------------------------------
//            class gbtNfgSupport: Data access -- strategies
//--------------------------------------------------------------------------

int gbtNfgSupport::NumStrats(int pl) const
{
  int total = 0;
  for (int st = 1; st <= m_strategies.Lengths()[pl]; st++) {
    total += m_strategies(pl, st);
  }
  return total;
}

gbtArray<int> gbtNfgSupport::NumStrats(void) const
{
  gbtArray<int> ret(m_nfg.NumPlayers());

  for (int pl = 1; pl <= ret.Length(); pl++) {
    ret[pl] = NumStrats(pl);
  }
  return ret;
}

int gbtNfgSupport::ProfileLength(void) const
{
  int total = 0;
  for (int i = 1; i <= m_strategies.Length(); i++) {
    total += m_strategies[i];
  }
  return total;
}

gbtNfgStrategy gbtNfgSupport::GetStrategy(int pl, int st) const
{
  int index = 0;
  for (int i = 1; i <= m_nfg.NumStrats(pl); i++) {
    if (m_strategies(pl, i)) {
      if (++index == st) {
	return m_nfg.GetPlayer(pl).GetStrategy(i);
      }
    }
    
  }
  return 0;
}

int gbtNfgSupport::GetIndex(gbtNfgStrategy p_strategy) const
{
  int pl = p_strategy.GetPlayer().GetId();
  for (int st = 1; st <= NumStrats(pl); st++) {
    if (GetStrategy(pl, st) == p_strategy) {
      return st;
    }
  }
  return 0;
}

bool gbtNfgSupport::Contains(gbtNfgStrategy p_strategy) const
{
  return m_strategies(p_strategy.GetPlayer().GetId(), p_strategy.GetId());
}

//--------------------------------------------------------------------------
//                  class gbtNfgSupport: Manipulation
//--------------------------------------------------------------------------

void gbtNfgSupport::AddStrategy(gbtNfgStrategy s)
{
  m_strategies(s.GetPlayer().GetId(), s.GetId()) = 1;
}

void gbtNfgSupport::RemoveStrategy(gbtNfgStrategy s)
{
  m_strategies(s.GetPlayer().GetId(), s.GetId()) = 0;
}

//--------------------------------------------------------------------------
//           class gbtNfgSupport: Data access -- properties
//--------------------------------------------------------------------------

bool gbtNfgSupport::IsSubset(const gbtNfgSupport &s) const
{
  if (m_nfg != s.m_nfg)  return false;
  for (int pl = 1; pl <= m_nfg.NumPlayers(); pl++) {
    if (NumStrats(pl) > s.NumStrats(pl)) {
      return false;
    }

    for (int st = 1; st <= m_nfg.NumStrats(pl); st++) {
      if (m_strategies(pl, st) && !s.m_strategies(pl, st)) {
	return false;
      }
    }
  }
  return true;
}

bool gbtNfgSupport::IsValid(void) const
{
  for (int pl = 1; pl <= m_nfg.NumPlayers(); pl++) {
    if (NumStrats(pl) == 0) {
      return false;
    }
  }
  return true;
}

void gbtNfgSupport::Output(gbtOutput &p_output) const
{
  p_output << '"' << m_label << "\" { ";
  for (int pl = 1; pl <= GetGame().NumPlayers(); pl++) {
    p_output << "{ ";
    for (int st = 1; st <= NumStrats(pl); st++) {
      p_output << "\"" << GetStrategy(pl, st).GetLabel() << "\" ";
    }
    p_output << "} ";
  }
  p_output << "} ";
}

gbtOutput& operator<<(gbtOutput &p_stream, const gbtNfgSupport &p_support)
{
  p_support.Output(p_stream);
  return p_stream;
}
