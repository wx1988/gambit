//#
//# FILE: player.h -- Declaration of Player data type
//#
//# $Id$
//#

#ifndef PLAYER_H
#define PLAYER_H

class gRational;

class EFPlayer   {
  friend class BaseEfg;
  friend class Efg<double>;
  friend class Efg<gRational>;
  private:
    int number;
    gString name;
    BaseEfg *E;
    
    gBlock<Infoset *> infosets;

    EFPlayer(BaseEfg *e, int n) : number(n), E(e)  { }
    ~EFPlayer();

  public:
    BaseEfg *BelongsTo(void) const   { return E; }
  
    const gString &GetName(void) const   { return name; }
    void SetName(const gString &s)       { name = s; }

    bool IsChance(void) const      { return (number == 0); }

    int NumInfosets(void) const    { return infosets.Length(); }

    Infoset *GetInfoset(const gString &name) const;
    const gArray<Infoset *> &InfosetList(void) const  { return infosets; }

    bool IsInfosetDefined(const gString &) const;

// This function exists only to facilitate the current version of efg2nfg
// and will not exist in the future.  DO NOT USE IN NEW CODE!
    int GetNumber(void) const    { return number; }
};


#endif    //# PLAYER_H


