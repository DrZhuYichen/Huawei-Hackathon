#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <list>
#include <queue>
#include <set>
#include <map>
#include <algorithm>
#include <chrono>
#include <cassert>

using namespace std;

const unsigned MAX_DIST = 7;
const unsigned MAX_SIZE = 64; // One graph max number of nodes.
const bool DEBUG = true;

struct Node {
	string OpCode;
	unsigned ID;
	vector<Node*> Operands;
	vector<Node*> Users;
	Node *Prev;
	Node *Next;

	explicit Node(unsigned id, string opCode = "") :
		ID(id), OpCode(opCode), Prev(nullptr), Next(nullptr) { }

	/// Replace this node's uses to given node.
	void replaceAllUsesWith(Node *node) {
		for (Node *user : this->Users)
			for (Node *operand : user->Operands)
				if (operand == this)
					operand = node;
	}

	/// Replace this node's uses to given node, only for user is in given include vector.
	void replaceAllUsesInclude(Node *node, vector<Node*> include) {
		for (Node *user : this->Users) {
			if (find(include.begin(), include.end(), user) != include.end()) {
				vector<Node*> Operands(user->Operands);
				for (Node *operand: Operands) {
					if (operand == this) {
						operand = node;
						this->Users.erase(find(this->Users.begin(), this->Users.end(), user));
						node->Users.push_back(user);
						user->Operands.erase(find(user->Operands.begin(), user->Operands.end(), this));
						user->Operands.push_back(node);
					}
				}
			}
		}
	}

	/// Move this node after given node.
	void moveAfter(Node *node) {
		if (node->Next) {
			this->Next = node->Next;
			node->Next->Prev = this;
			node->Next = this;
			this->Prev = node;
		} else { // last node
			node->Next = this;
			this->Prev = node;
		}
	}

	/// Move this node before given node.
	void moveBefore(Node *node) {
		if (node->Prev == node) { // first node
			this->Prev = this;
			this->Next = node;
			node->Prev = this;
		} else {
			this->Prev = node->Prev;
			this->Next = node;
			node->Prev->Next = this;
			node->Prev = this;
		}
	}
};

class Solution {
public:
	void loadFile(string fileName) {
		ifstream input(fileName);

		if (!input.is_open())
			assert(0 && "Can not open input file!\n");

		string line;
		for (unsigned i = 0; !input.eof(); ++i) {
			Node *node = new Node(i, "op" + to_string(i));
			if (i != 0) {
				node->Prev = graph.back();
				node->Prev->Next = node;
			}

			string operandStr;
			while (char c = input.get()) {
				if (c == '\n' || c == EOF)
					break;
				if (c == ' ')
					continue;
				input >> operandStr;
				assert(c == '#' && "Wrong node input format!");
				unsigned id = stoi(operandStr.c_str());
				Node *define = getPrevNode(node, id);
				node->Operands.push_back(define);
				if (define->ID != node->ID)
					define->Users.push_back(node);
			}

			graph.push_back(node);
		}
		assert(graph.size() <= MAX_SIZE && "Input graph size over MAX_SIZE!");

		input.close();
	}

	void printNodesWithDotFormat() {
		printNodesWithDotFormat(graph);
	}

	/// Main Function
	void updateNodes() {
		auto start = chrono::steady_clock::now();
		if (isSatisfiesNodes(graph)) {
			if (DEBUG)
				cout << "Great! The input graph need not insert any node!\n";
			return; // Do not insert any node.
		}

		list<Node*> resultGraph(deepCopy(graph));
		bool findSolution = findOptimalSolution(graph, resultGraph);
		if (findSolution) {
			if (DEBUG)
				cout << "Lucky! After adjust the order of the graph, " <<
				     "the new graph need not insert any node!\n";

			graph = resultGraph;
		} else {
			if (DEBUG)
				cout << "Sad... We can only insert new node for input graph.\n";

			unsigned iterCount = 0;
			list<Node*> targetGraph(deepCopy(graph));
			do {
				assert(iterCount < 2 && "Find corner case! Need more test!");
				insertNodes(targetGraph);
				findSolution = findOptimalSolution(targetGraph, resultGraph);
				iterCount++;
				if (DEBUG)
					cout << "At " <<  iterCount << " iteration, has insert " <<
									resultGraph.size() - graph.size() << " nodes.\n";
			} while (!findSolution);

			if (DEBUG)
				cout << "After insert " << resultGraph.size() - graph.size() <<
								" nodes, find optimal solution, which pass " << iterCount << " times iterations.\n";

			graph = resultGraph;
		}

		auto end = chrono::steady_clock::now();;
		long long times = chrono::duration_cast<chrono::microseconds>(end - start).count() / 1000;
		if (DEBUG)
			cout << "Whole program take: " << times << " ms.\n";
	}

private:
	void printNodesWithDotFormat(list<Node*> graph) {
		cout << "---------- graph with dot format ----------\n";
		cout << "digraph nodes { \n";
		vector<Node*> copyNodes;
		for (Node *node : graph) {
			if (node->OpCode == "copy")
				copyNodes.push_back(node);
			for (Node *user : node->Users) {
				cout << "  " << node->ID << "->" << user->ID << endl;
			}
		}

		for (Node *node : copyNodes)
			cout << "  " <<node->ID << " [color=red]\n";

		cout << "}\n";
		cout << "----------          End          ----------\n";
	}

	/// Design for converting nodes to graph.
	Node* getPrevNode(Node* node, unsigned n = 1) {
		while (n != 0) {
			assert(node && "Any Node should define firstly then use!");
			node = node->Prev;
			n--;
		}
		return node;
	}

	/// Check if the every node in graph satisfy:
	/// 1. Define always before use.
	/// 2. If checkDist=T, the distance between use and define should less than MAX_DIST.
	bool isSatisfiesNodes(const list<Node*>& graph, bool checkDist = true) {
		for (Node *node : graph) {
			unsigned curID = node->ID;
			for (Node *operand : node->Operands) {
				unsigned operandID = operand->ID;
				if (operandID > curID)
					return false;
				if (checkDist && curID - operandID > MAX_DIST)
					return false;
			}
		}
		return true;
	}

	/// Get the critical path of given node in graph, which use for pruning optimal in DFS travel.
	unsigned getLengthOfCriticalPath(Node *node, list<Node*> graph) {
		queue<Node*> Q;
		Q.push(node);
		Node *lastNode = node;
		unsigned path = -1;
		while (!Q.empty()) {
			Node *curNode = Q.front();
			Q.pop();
			for (Node *n : curNode->Users)
				Q.push(n);

			if (curNode == lastNode) {
				lastNode = Q.back();
				path++;
			}
		}
		return path;
	}

	/// Maybe one day, create struct: Graph for nodes, and overload this assignment copy function.
	list<Node*> deepCopy(const list<Node*>& target) {
		list<Node*> result;
		map<Node*, Node*> nodeMap;
		for (Node *node : target) {
			Node *copyNode = new Node(node->ID, node->OpCode);
			nodeMap[node] = copyNode;
			result.push_back(copyNode);
		}
		for (Node *node : target) {
			Node *curNode = nodeMap[node];
			for (Node *operand : node->Operands)
				curNode->Operands.push_back(nodeMap[operand]);
			for (Node *user : node->Users)
				curNode->Users.push_back(nodeMap[user]);
			curNode->Prev = nodeMap[node->Prev];
			curNode->Next = nodeMap[node->Next];
		}
		return result;
	}

	/// Core of algorithm: travel the whole graph and get satisfy ids for every nodes.
	void dfsTravel(unsigned traveledNum, Node* curNode, set<unsigned> candidateIDs,
	               list<Node*>& curGraph, list<Node*>& result, map<Node*, unsigned>& longestPathMap,
								 bool &findSolution, bool needOptimalSolution) {
		if (findSolution)
			return;
		unsigned graphSize = curGraph.size();
		if (traveledNum == graphSize) {
			if (isSatisfiesNodes(curGraph, needOptimalSolution)) { // Find solution
				result = deepCopy(curGraph);
				findSolution = true;
			}
			return;
		}

		set<unsigned> possibleIDs(candidateIDs);
		unsigned longestPath = 0;
		if (longestPathMap.count(curNode)) {
			longestPath = longestPathMap[curNode];
		} else {
			longestPath = getLengthOfCriticalPath(curNode, curGraph);
			longestPathMap[curNode] = longestPath;
		}
		for (unsigned id : possibleIDs) {
			if (id + longestPath >= graphSize)
				continue; // prune optimal.

			unsigned temp = curNode->ID;
			curNode->ID = id;
			bool isSatisfied = true;
			for (Node *operand : curNode->Operands) {
				if (operand->ID < UINT8_MAX &&
						(operand->ID > id || (needOptimalSolution && id - operand->ID > MAX_DIST))) {
					isSatisfied = false;
					break;
				}
			}
			for (Node *user : curNode->Users) { // Allow travel out of order.
				if (user->ID < UINT8_MAX &&
						(user->ID < id || (needOptimalSolution && user->ID - id > MAX_DIST))) {
					isSatisfied = false;
					break;
				}
			}
			if (isSatisfied) {
				candidateIDs.erase(id);
				dfsTravel(traveledNum + 1, curNode->Next, candidateIDs,curGraph,
									result, longestPathMap, findSolution, needOptimalSolution);
				candidateIDs.insert(id); // Backtracking
			}
			curNode->ID = temp; // Backtracking
		}
	}

	/// Design for calling DFS travel function.
	/// Cause DFS travel function will remark all node ID in graph, in this function, deep copy
	/// the graph, and set all nodes ID to UINT8_MAX, which represent this note have not be marked.
	bool findOptimalSolution(const list<Node*>& target, list<Node*> &result) {
		auto initialGraph = [&] (list<Node*>& targetGraph, set<unsigned>& candidateIDs) {
			candidateIDs.clear();
			unsigned initialID = 0;
			for (Node *node : targetGraph) {
				candidateIDs.insert(initialID++);
				node->ID = UINT8_MAX;
			}
		};

		set<unsigned> candidateIDs;
		list<Node*> temp(deepCopy(target));
		initialGraph(temp, candidateIDs);
		bool findSolution = false;
		map<Node*, unsigned> longestPathMap;
		auto start = chrono::steady_clock::now();
		dfsTravel(0, temp.front(), candidateIDs, temp,
		          result, longestPathMap, findSolution, true);
		auto end = chrono::steady_clock::now();
		long long times = chrono::duration_cast<chrono::microseconds>(end - start).count() / 1000;
		if (DEBUG)
			cout << "DFS find optimal solution take: " << times << " ms.\n";
		if (!findSolution) {
			temp = deepCopy(target);
			initialGraph(temp, candidateIDs);
			start = chrono::steady_clock::now();
			dfsTravel(0, temp.front(), candidateIDs, temp,
			          result, longestPathMap, findSolution, false);
			end = chrono::steady_clock::now();
			long long times = chrono::duration_cast<chrono::microseconds>(end - start).count() / 1000;
			if (DEBUG)
				cout << "DFS find possible solution take: " << times << " ms.\n";
			return false;
		} else {
			return true;
		}
	}

	/// Insert node if the distance between use and define over MAX_DIST. Note:
	/// 1. If distance more than MAX_DIST N times, using multi insert nodes.
	/// 2. If one define node have multi uses which distance over MAX_DIST, share common nodes.
	/// 3. After node insert, all nodes in graph will be initialized to UINT8_MAX.
	void insertNodes(list<Node*> &curGraph) {
		map<Node*, map<unsigned, vector<Node*>, less<>>> m;
		for (Node *node : curGraph) {
			map<unsigned, vector<Node*>, less<>> userInfo;
			unsigned curID = node->ID;
			for (Node *user : node->Users)
				if (user->ID - curID > MAX_DIST)
					userInfo[(user->ID - curID) / MAX_DIST].push_back(user);

			if (!userInfo.empty())
				m[node] = userInfo;
		}

		unsigned allInsertNodesNum = 0; // use for debug mode to see dot file.
		for (auto info : m) {
			Node *node = info.first;
			unsigned insertNodeNum = 0;
			unsigned insertNodeCount = 0;
			Node *prevNode = node;
			// Note: Create new node from short distance users to long distance users.
			// Small distance users share common node for long distance users.
			vector<Node*> include;
			for (auto userInfo : info.second) // Collect all users.
				include.insert(include.end(), userInfo.second.begin(), userInfo.second.end());
			for (auto userInfo : info.second) {
				unsigned needNodesNum = userInfo.first - insertNodeNum;
				while (needNodesNum--) {
					Node *insertNode = new Node(UINT8_MAX + allInsertNodesNum, "copy");
					allInsertNodesNum++;
					insertNode->moveAfter(node);
					curGraph.insert(next(find(curGraph.begin(), curGraph.end(), node)), insertNode);
					insertNode->Operands = {prevNode};
					prevNode->replaceAllUsesInclude(insertNode, include);
					prevNode->Users.push_back(insertNode);
					prevNode = insertNode;
					insertNodeNum++;
					insertNodeCount++;
					if (insertNodeCount == MAX_DIST) {
						needNodesNum++;
						insertNodeCount = 0;
					}
				}
				for (Node *node : userInfo.second)
					include.erase(find(include.begin(), include.end(), node));
			}
		}
	}

	list<Node*> graph;
};

int main() {
	Solution solution;
	solution.loadFile("graph1.txt");
	solution.printNodesWithDotFormat();
	solution.updateNodes();
	solution.printNodesWithDotFormat();
	return 0;
}
