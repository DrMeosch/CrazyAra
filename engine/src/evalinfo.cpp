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
 * @file: evalinfo.cpp
 * Created on 13.05.2019
 * @author: queensgambit
 */

#include "evalinfo.h"
#include "uci.h"
#include "../util/blazeutil.h"

void print_single_pv(std::ostream& os, const EvalInfo& evalInfo, size_t idx, size_t elapsedTimeMS)
{
    if (idx != 0) {
        os << "info score ";
    }
    os << "multipv " << idx+1;
    if (evalInfo.movesToMate[idx] == 0) {
       os << " cp " << evalInfo.centipawns[idx];
    }
    else {
       os << " mate " << evalInfo.movesToMate[idx];
    }
    os << " depth " << evalInfo.depth
       << " seldepth " << evalInfo.selDepth
       << " nodes " << evalInfo.nodes
       << " time " << elapsedTimeMS
       << " nps " << evalInfo.calculate_nps(elapsedTimeMS)
       << " tbhits " << evalInfo.tbHits
       << " pv";
    for (Move move: evalInfo.pv[idx]) {
        os << " " << UCI::move(move, evalInfo.isChess960);
    }
    os << endl;
}

std::ostream& operator<<(std::ostream& os, const EvalInfo& evalInfo)
{
    const size_t elapsedTimeMS = evalInfo.calculate_elapsed_time_ms();

    for (size_t idx = 0; idx < evalInfo.centipawns.size(); ++idx) {
        print_single_pv(os, evalInfo, idx, elapsedTimeMS);
    }
    return os;
}

size_t EvalInfo::calculate_elapsed_time_ms() const
{
    return chrono::duration_cast<chrono::milliseconds>(end - start).count();
}

size_t EvalInfo::calculate_nps(size_t elapsedTimeMS) const
{
    // avoid division by 0
    if (elapsedTimeMS == 0) {
        elapsedTimeMS = 1;
    }
    return int(((nodes-nodesPreSearch) / (elapsedTimeMS / 1000.0f)) + 0.5f);
}

size_t EvalInfo::calculate_nps() const
{
    return calculate_nps(calculate_elapsed_time_ms());
}

// https://helloacm.com/how-to-implement-the-sgn-function-in-c/
template <class T>
inline int
sgn(T v) {
    return (v > T(0)) - (v < T(0));
}

int value_to_centipawn(float value)
{
    if (std::abs(value) >= 1) {
        // return a constant if the given value is 1 (otherwise log will result in infinity)
        return sgn(value) * 9999;
    }
    // use logarithmic scaling with basis 1.1 as a pseudo centipawn conversion
    return int(-(sgn(value) * std::log(1.0f - std::abs(value)) / std::log(1.2f)) * 100.0f);
}

bool set_eval_for_single_pv(EvalInfo& evalInfo, Node* rootNode, size_t idx, vector<size_t>& indices)
{
    vector<Move> pv;
    size_t childIdx;
    if (idx == 0) {
        childIdx = get_best_move_index(rootNode, false);
    }
    else {
        childIdx = indices[idx];
    }
    pv.push_back(rootNode->get_move(childIdx));
    const Node* nextNode = rootNode->get_child_node(childIdx);
    if (nextNode == nullptr || !nextNode->is_playout_node()) {
        evalInfo.movesToMate.resize(idx);
        evalInfo.bestMoveQ.resize(idx);
        evalInfo.centipawns.resize(idx);
        return false;
    }
    nextNode->get_principal_variation(pv);
    evalInfo.pv.emplace_back(pv);

    // scores
    // return mate score for known wins and losses
    if (nextNode->get_node_type() == SOLVED_LOSS) {
        // always round up the ply counter
        evalInfo.movesToMate[idx] = pv.size() / 2 + evalInfo.pv.size() % 2;
    }
    else if (nextNode->get_node_type() == SOLVED_WIN) {
        // always round up the ply counter
        evalInfo.movesToMate[idx] = -pv.size() / 2 + evalInfo.pv.size() % 2;
    }
    else {
        evalInfo.movesToMate[idx] = 0;
        evalInfo.bestMoveQ[idx] = rootNode->get_q_value(childIdx);
        evalInfo.centipawns[idx] = value_to_centipawn(evalInfo.bestMoveQ[idx]);
    }
    return true;
}

void sort_eval_lists(EvalInfo& evalInfo, vector<size_t>& indices)
{
    auto p = sort_permutation(evalInfo.policyProbSmall, std::greater<float>());
    for (size_t idx = 0; idx < evalInfo.legalMoves.size(); ++idx) {
        indices.emplace_back(idx);
    }
    apply_permutation_in_place(evalInfo.policyProbSmall, p);
    apply_permutation_in_place(evalInfo.legalMoves, p);
    apply_permutation_in_place(indices, p);
}

void update_eval_info(EvalInfo& evalInfo, Node* rootNode, size_t tbHits, size_t selDepth)
{
    evalInfo.childNumberVisits = rootNode->get_child_number_visits();
    evalInfo.policyProbSmall.resize(rootNode->get_number_child_nodes());
    size_t bestMoveIdx;
    rootNode->get_mcts_policy(evalInfo.policyProbSmall, bestMoveIdx);
    evalInfo.policyProbSmall.resize(rootNode->get_number_child_nodes());
    evalInfo.legalMoves = rootNode->get_legal_moves();

    vector<size_t> indices;
    size_t maxIdx = min(evalInfo.multiPV, evalInfo.legalMoves.size());

    if (maxIdx > 1) {
        sort_eval_lists(evalInfo, indices);
    }

    evalInfo.pv.clear();
    evalInfo.movesToMate.resize(maxIdx);
    evalInfo.bestMoveQ.resize(maxIdx);
    evalInfo.centipawns.resize(maxIdx);

    for (size_t idx = 0; idx < maxIdx; ++idx) {
        if (!set_eval_for_single_pv(evalInfo, rootNode, idx, indices)) {
            break;
        }
    }

    evalInfo.depth = evalInfo.pv[0].size();
    evalInfo.selDepth = selDepth;
    evalInfo.nodes = get_node_count(rootNode);
    evalInfo.tbHits = tbHits;
}
