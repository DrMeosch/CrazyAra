/*
  CrazyAra, a deep learning chess variant engine
  Copyright (C) 2018       Johannes Czech, Moritz Willig, Alena Beyer
  Copyright (C) 2019-2020  Johannes Czech

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/*
 * @file: nodedata.cpp
 * Created on 25.04.2020
 * @author: queensgambit
 */

#include "nodedata.h"
#include "util/blazeutil.h"

void NodeData::add_empty_node()
{
    append(childNumberVisits, 0U);
    append(actionValues, 0.0f);
    append(qValues, Q_INIT);
    childNodes.emplace_back(nullptr);
}

void NodeData::reserve_initial_space()
{
    const int initSize = min(PRESERVED_ITEMS, int(numberUnsolvedChildNodes));  //numberChildNodes

    // # visit count of all its child nodes
    childNumberVisits.reserve(initSize);

    // total action value estimated by MCTS for each child node also denoted as w
    actionValues.reserve(initSize);

    // q: combined action value which is calculated by the averaging over all action values
    // u: exploration metric for each child node
    // (the q and u values are stacked into 1 list in order to speed-up the argmax() operation
    qValues.reserve(initSize);

    childNodes.reserve(initSize);

    add_empty_node();
}

NodeData::NodeData(size_t numberChildNodes):
    terminalVisits(0),
    checkmateIdx(NO_CHECKMATE),
    endInPly(0),
    noVisitIdx(1),
    numberUnsolvedChildNodes(numberChildNodes),
    nodeType(UNSOLVED)
{
    // specify the number of direct child nodes of this node
    numberUnsolvedChildNodes = numberChildNodes;

    reserve_initial_space();
}

auto NodeData::get_q_values()
{
    return blaze::subvector(qValues, 0, noVisitIdx);
}

