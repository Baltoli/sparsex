/*
 * Copyright (C) 2009-2012, Computing Systems Laboratory (CSLab), NTUA.
 * Copyright (C) 2009-2011, Kornilios Kourtis
 * Copyright (C) 2009-2012, Vasileios Karakasis
 * Copyright (C) 2010-2012, Theodoros Gkountouvas
 * All rights reserved.
 *
 * This file is distributed under the BSD License. See LICENSE.txt for details.
 */

/**
 * \file Node.hpp
 * \brief Node class
 *
 * \author Computing Systems Laboratory (CSLab), NTUA
 * \date 2011&ndash;2014
 * \copyright This file is distributed under the BSD License. See LICENSE.txt
 * for details.
 */

#ifndef SPARSEX_INTERNALS_NODE_HPP
#define SPARSEX_INTERNALS_NODE_HPP

#include <sparsex/internals/Encodings.hpp>
#include <iostream>
#include <set>
#include <map>
#include <inttypes.h>

using namespace std;

namespace sparsex {
  namespace csx {

    /**
     *  Keeps data of an encoding sequence.
     */
    class Node {
    public:
      Node(uint32_t depth);
      ~Node() {}
    
      void PrintNode();

      /**
       *  Ignore the type for the encoding sequence examined.
       *
       *  @param type type which is ignored.
       */
      void Ignore(Encoding::Type type);

      /**
       *  Copies a node to a new one and inserts an extra type in the end of the
       *  encoding sequence.
       *  
       *  @param type   type which is inserted in the end.
       *  @param deltas deltas corresponding to type inserted.
       */
      Node MakeChild(Encoding::Type type, set<uint64_t> deltas);

    private:
      uint32_t depth_;
      map<Encoding::Type, set<uint64_t> > deltas_path_;
      Encoding::Type *type_path_;
      Encoding::Type *type_ignore_;
      template<typename IndexType, typename ValueType>
      friend class EncodingManager;
    };

  } // end of namespace csx
} // end of namespace sparsex

#endif  // SPARSEX_INTERNALS_NODE_HPP
