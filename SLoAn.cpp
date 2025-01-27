﻿#include "SLoAn.h"

int main(int argc, char* argv[])
{
	const bool debug = true;
	path path_to_wordlist{ "db" };		// hardcoded, for now
	path_to_wordlist /= "2of12inf.txt";
	data* const& map = new data();

	bool lookingForFile = true;
	while (lookingForFile) {
		const path absolute_path_to_wordlist = boost::filesystem::current_path().string() / path_to_wordlist;
		if (!fileExists(absolute_path_to_wordlist))
		{
			cout << "Cannot open " << absolute_path_to_wordlist << endl;
			EXIT_FAILURE;
		}

		map->setFile(absolute_path_to_wordlist);
		cout << "Loading " << absolute_path_to_wordlist << "..." << endl;
		clock_t t = clock();
		try {
			data::build(map, debug);
		}
		catch (invalid_argument& e) {
			cout << e.what() << endl;
			while (bool query = true) {
				char input[1];
				cout << "Retry? (y/n): ";
				cin >> input;
				cout << endl;
				if (std::tolower(input[0]) == 'y') {
					query = false;
				}
				else if (std::tolower(input[0]) == 'n') {
					cout << "Program closed" << endl;
					EXIT_SUCCESS;
				}
			}
			continue;
		}
		lookingForFile = false;

		t = clock() - t;

		if (debug)
			cout << "Constructed hashmap in approximately " << (float)t / CLOCKS_PER_SEC << "seconds" << endl;

		string filename = map->getFile_path().filename().string();
		if (map->isModified() || !map->isSaved()) {
			map->write(filename);
		}
	}

	string input = queryInput();

	string sorted_input = parseInput(input);										// allow only permitted characters in input string
	sort(sorted_input.begin(), sorted_input.end());									// sort input string in alphabetical order
	auto solution_it = solve(map, sorted_input, 2, debug);
	
	if (solution_it != map->getSolutionMap()->end()) {
		vector<vector<string>> solution_arr = traverse(solution_it->second);
		logSolutions(map, solution_arr);
	}

	return 0;
}

/// <summary>
/// Returns true if a file exists at the provided file path.
/// The path is relative from the current working directory.
/// </summary>
/// <param name="name">The path to the file to be checked relative from the current working directory</param>
/// <returns>True, if a file exists at the location and false otherwise</returns>
bool fileExists(const path& name)
{
	struct stat buffer;
	return (stat(name.string().c_str(), &buffer) == 0);
}

string queryInput()
{
	int max = 64;
	bool validInput = false;
	string input;

	while (!validInput) {
		try {
			cout << "Enter text: ";
			input = receiveInput(max);
			cout << "\n";
			input = parseInput(input);
			validInput = true;
		}
		catch (const std::invalid_argument& e)
		{
			cout << &e << "\n";
		}
	}

	return input;
}

string receiveInput(int max)
{
	if (max > 64)
		EXIT_FAILURE;									// input array cannot be initialized with a larger size than 64 elements

	char input[66];
	cin.get(input, 66);
	if (strlen(input) > max) {
		string error_msg = "Exceeded input character limit. No more than ";
		error_msg += max;
		error_msg += " characters are allowed";

		throw new invalid_argument(error_msg);
	}

	return string(input);
}

string parseInput(string input)
{
	string parsedInput, invalid_chars;
	bool invalid_argument = false;

	for (char const& c : input) {
		if (isalpha(c)) {						// alphabetic symbols are allowed and included
			parsedInput += c;
		}
		else if (isspace(c) || ispunct(c)) {	// whitespace and punctuation is allowed but excluded
			continue;
		}
		else {									// any other characters are not allowed
			invalid_argument = true;
			if (invalid_chars.find(c) != string::npos) {	// if c is not yet in invalid_chars
				invalid_chars += c;							// then add c
			}
		}
	}

	if (invalid_argument) {
		string error_msg;
		if (invalid_chars.length() == 1) {
			error_msg = "Symbol '" + invalid_chars + "' is not allowed as part of the input";
		}
		else {
			error_msg = "Several symbols are not allowed as part of the input: ";

			for (string::size_type i = 0; i < invalid_chars.length(); i++) {
				char c = invalid_chars[i];
				error_msg += "'" + c;
				error_msg += "'";
				if (invalid_chars.length() > i + 1) {
					error_msg += ", ";
				}
			}
		}
		throw std::invalid_argument(error_msg);
	}

	return parsedInput;
}

string maskString(string& str, int mask[])
{
	size_t size = sizeof(*mask) / sizeof(mask[0]);
	string sub_str;
	for (int i = 0; i < size; i++) {
		if (mask[i] == 1)
			sub_str += str.at(i);
	}
	return sub_str;
}

unordered_map<string, vector<shared_ptr<keynode>>>::const_iterator solve(data* const& map, const string& input, const int min_solution_length, const bool debug)
{
	shared_ptr<keynode> root = make_shared<keynode>(keynode(input));
	solve_rec(root, map, input, min_solution_length, debug);
	return map->findSolution(input);
}

void debugFunction(shared_ptr<keynode> node_ptr)
{
	string test_node = "parliament";
	sort(test_node.begin(), test_node.end());
	if (test_node == node_ptr->key) {
		vector<shared_ptr<keynode>> temp;
		temp.push_back(node_ptr);
		traverse(temp);
	}
}

bool solve_rec(shared_ptr<keynode> node_ptr, data* const& map, const string& seq, const int min_solution_length, const bool debug)
{
	unordered_map<string, vector<shared_ptr<keynode>>>* solutionMap = map->getSolutionMap();
	if (seq.size() >= min_solution_length) {
		auto it_seq = solutionMap->find(seq);
		if (it_seq != solutionMap->end()) {										// found an pre-existing solution
			return checkSolutions(it_seq, node_ptr);							// check if a solution exists for "seq." If so, add the node for that solution to "node_ptr" and return true
		}
	}

	bool is_solution = false;
	unordered_map<string, vector<string>>* anagramMap = map->getAnagramMap();
	int n = (1 << seq.length()) - 1;										// n is the maximum iterations
	for (int mask = n; mask > 0; mask--) {									// mask will be every possible configuration of 0's and 1's in an size n binary sequence (except the sequence = 0)	
		string subseq_in = "", subseq_out = "";								// subseq_in is the subsequence of input that is within the mask, while -out is outside of the mask
		maskSequence(seq, mask, &subseq_in, &subseq_out);					// mask the input and collect the subsequence in the belonging string subseq_in or -out
																			// i is the index of input where we stopped masking, because subseq_in is complete		
		if (node_ptr->children.find(subseq_in) != node_ptr->children.end()) {	// if the "subseq_in" is already known by the node (this happens with duplicate letters in input)
			continue;														// solve the next sequence
		}

		if (subseq_in.size() >= min_solution_length) {
			//!compare performance between using solutionMap always vs. only if subseq_in.size() >= 1
			auto it_in = solutionMap->find(subseq_in);						// check if the same problem has already been solved. if so, then the answer is stored in solutionMap
			if (it_in != solutionMap->end()) {								// if the solution to the problem is known
				if (it_in->second.size() != 0) {							// and if the solution yields any anagrams for this sequence ("no anagram" solution is stored as empty vector)
					is_solution |= solveRemainingLetters(map, node_ptr, seq, subseq_out, subseq_in, it_in, min_solution_length, debug);
				}
				continue;													// solve the next subsequence
			}
		}

		auto anagram_it = anagramMap->find(subseq_in);						// get all anagrams of subseq_in
		if (anagram_it != anagramMap->end()) {								// if there is any anagram at all, then we'll go a recursion deeper
			map->addSolution(subseq_in, subseq_in, min_solution_length);	// save this solution for later
			shared_ptr<keynode> child_ptr = make_shared<keynode>(keynode(subseq_in));
			if (subseq_out.size() == 0) {									// if all letters in the input have been used in the sequence
				keynode::add(child_ptr, node_ptr);							// add the new child node to this node
				is_solution = true;											// indicate to caller that a solution has been found
				debugFunction(node_ptr);
			}
			else if (solve_rec(child_ptr, map, subseq_out, min_solution_length, debug)) {	// if all parts in the sequence form a solution
				map->addSolution(seq, child_ptr, min_solution_length);		// add this solution to the subsequence to the map of known solutions
				keynode::add(child_ptr, node_ptr);							// then add all those parts to this node
				is_solution = true;											// indicate that this subtree contains at least 1 solution
				debugFunction(node_ptr);
			}
		}
		else {																// if there is ultimately no anagram found for this key
			map->addEmptySolution(subseq_in, min_solution_length);			// add the "no anagram" solution for this key
		}
	}

	return is_solution;														// tell caller whether this subsequence contains any solutions
}

void maskSequence(const string seq, int mask, string* const& subseq_in, string* const& subseq_out) {
	int r = 0;																// r is the remainder
	unsigned long long i = 0ull;											// i is the index of the current character of input
																			// mask stands in for the dividend
	while (mask > 0) {														// in this loop we read out every binary digit in the binary sequence of mask
		r = mask % 2;														// read out the digit and store in r
		mask >>= 1;															// shift to the next (which is the same as: divide by 2)
		((r == 1) ? *subseq_in : *subseq_out) += seq.at(i);					// store the char in either subseq_in or subseq_out according to the mask
		i++;																// increment i and iterate
	}
	*subseq_out += seq.substr(i);											// if any, append remaining characters of input to subseq_out
}

void eraseSolution(string key, unordered_map<string, vector<shared_ptr<keynode>>>* solutionMap)
{
	auto nodes = solutionMap->find(key);
	if (nodes != solutionMap->end()) {
		nodes->second.pop_back();											// remove the last added solution to input: the node referenced by child_ptr
		if (nodes->second.size() == 0) {
			solutionMap->erase(nodes);
		}
	}
}

bool contains(vector<shared_ptr<keynode>> nodes, string key)
{
	for (shared_ptr<keynode> node_ptr : nodes) {
		if (node_ptr->key == key)
			return true;
	}
	return false;
}

bool solve_intermediary_node_v1(const string& intermediary_key, shared_ptr<keynode> parent, const string& seq, const string& subseq_out, data* const& map, const int min_solution_length, const bool debug) {
	shared_ptr<keynode> temp_node = make_shared<keynode>(keynode(intermediary_key));
	if (solve_rec(temp_node, map, subseq_out, min_solution_length, debug)) {
		keynode::add(temp_node, parent);
		map->addSolution(seq, temp_node, min_solution_length);
		return true;
	}
	return false;
}

bool solve_intermediary_node_v2(shared_ptr<keynode> intermediary_node_ptr, shared_ptr<keynode> parent, const string& seq, const string& subseq_out, data* const& map, const int min_solution_length, const bool debug) {
	shared_ptr<keynode> temp_node = make_shared<keynode>(keynode(""));
	if (solve_rec(temp_node, map, subseq_out, min_solution_length, debug)) {
		for (auto it = temp_node->children.begin(); it != temp_node->children.end(); it++) {
			shared_ptr<keynode> child = it->second;
			
			if (parent->childKnown(child))
				continue;

			keynode::addToAllLeaves(intermediary_node_ptr, child);
			keynode::add(child, parent);
			map->addSolution(seq, child->key, min_solution_length);
		}
		return true;
	}
	return false;
}

bool checkSolutions(unordered_map<string, vector<shared_ptr<keynode>>>::const_iterator it, shared_ptr<keynode> node_ptr)
{
	if (it->second.size() != 0) {											// if the solution is that there are any anagrams
		for (shared_ptr<keynode> child_ptr : it->second) {
			keynode::add(child_ptr, node_ptr);								// add a reference for each existing child node to the tree					
		}
		return true;
	}
	else
		return false;
}

bool solveRemainingLetters(data* const& map, shared_ptr<keynode> node_ptr, const string& seq, const string& subseq_out, const string& subseq_in, unordered_map<string, vector<shared_ptr<keynode>>>::const_iterator it_in, const int min_solution_length, const bool debug)
{
	if (subseq_out.size() > 0) {											// if this isn't the complete solution to the sequence

						//TODO check if subseq_out in solutionMap
		unordered_map<string, vector<shared_ptr<keynode>>>* solutionMap = map->getSolutionMap();
		unordered_map<string, vector<shared_ptr<keynode>>>::const_iterator it_out = solutionMap->find(subseq_out);
		if (it_out != solutionMap->end() &&
			it_out->second.size() != 0 &&
			!map->eitherKeyIsInSolution(subseq_in, subseq_out, seq)) {
			// we found a new solution for seq by combining the already known solutions for subseq_in and -out

			// create a new node vector "parents" by duplicating either it_in->second or it_out->second and (including its children).
			// whether of the two has the lowest sum of n_leaves, or lowest sum of height as a tie breaker.
			auto t = bestParents(it_in->second, it_out->second);
			vector<shared_ptr<keynode>> parents = std::get<0>(t);
			vector<shared_ptr<keynode>> children = std::get<1>(t);
			const int sum_leaves = std::get<2>(t);
			const int max_height = std::get<3>(t);

			for (shared_ptr<keynode> parent : parents) {
				shared_ptr<keynode> parent_copy = parent->deepCopy();
				keynode::addToAllLeaves(children, parent_copy, sum_leaves, max_height);	// add the non-copied children as children to the leaves of "parents".
				map->addSolution(seq, parent_copy, min_solution_length);	// add "parents" to the solutionMap at key "seq"
			}

			return true;
		}

		else if (!contains(it_in->second, subseq_in)) {
			for (shared_ptr<keynode> child : it_in->second) {

				//TODO check if subseq_out among child descendants
				if (child->keyInDescendants(subseq_out)) {
					int a = 0;
				}

				return solve_intermediary_node_v2(child, node_ptr, seq, subseq_out, map, min_solution_length, debug);
			}
		}
		else {
			return solve_intermediary_node_v1(subseq_in, node_ptr, seq, subseq_out, map, min_solution_length, debug);
		}
	}
	else {
		for (shared_ptr<keynode> child_ptr : it_in->second) {
			keynode::add(child_ptr, node_ptr);								// add a reference for each existing child node to the tree
		}
		return true;														// indicate that this subtree contains at least 1 solution
	}
	return false;
}

tuple<vector<shared_ptr<keynode>>, vector<shared_ptr<keynode>>, int, int> bestParents(vector<shared_ptr<keynode>> nodes_x, vector<shared_ptr<keynode>> nodes_y)
{
	const int sum_leaves_x = keynode::sumLeaves(nodes_x);
	const int max_height_x = keynode::maxHeight(nodes_x);
	const int sum_leaves_y = keynode::sumLeaves(nodes_y);
	const int max_height_y = keynode::maxHeight(nodes_y);

	const int diff_sum_leaves = sum_leaves_x - sum_leaves_y;
	const int diff_max_height = max_height_x - max_height_y;

	if (diff_sum_leaves < 0) {												// sum of leaves in nodes_x is less than that in nodes_y
		return tuple<vector<shared_ptr<keynode>>, vector<shared_ptr<keynode>>, int, int>(nodes_x, nodes_y, sum_leaves_y, max_height_y);
	}
	else if (diff_sum_leaves > 0) {											// sum of leaves in nodes_x is greater than that in nodes_y
		return tuple<vector<shared_ptr<keynode>>, vector<shared_ptr<keynode>>, int, int>(nodes_y, nodes_x, sum_leaves_x, max_height_x);
	}																		// sum of leaves in nodes_x is equal to that in nodes_y
	else if (diff_max_height <= 0) {										// and sum of max_height in nodes_ x is less than or equal to that in nodes_y
		return tuple<vector<shared_ptr<keynode>>, vector<shared_ptr<keynode>>, int, int>(nodes_x, nodes_y, sum_leaves_y, max_height_y);
	}
	else {																	// sum of max_height in nodes_ x is greater than that in nodes_y
		return tuple<vector<shared_ptr<keynode>>, vector<shared_ptr<keynode>>, int, int>(nodes_y, nodes_x, sum_leaves_x, max_height_x);
	}
}

vector<vector<string>> traverse(vector<shared_ptr<keynode>> solutions_root)
{
	const size_t sum_leaves = keynode::sumLeaves(solutions_root);
	const size_t max_height = keynode::maxHeight(solutions_root);
	vector<vector<string>> arr(sum_leaves);
	vector<string> seq(max_height);

	int i_arr = 0;
	for (const shared_ptr<keynode> node : solutions_root) {
		assert(i_arr < sum_leaves && "There are more leaf nodes than space is reserved in the array");
		i_arr = node->traversePerNode(&arr, seq, i_arr);
	}

	return arr;
}

void logSolutions(data* const& map, vector<vector<string>> solution_arr)
{
	for (int i = 0; i < solution_arr.size(); i++) {
		cout << "Solution " << i << ":" << endl;
		vector<string> solution = solution_arr[i];

		for (string key : solution) {
			if (key == "")
				break;

			auto iterator = map->findAnagram(key);
			vector<string> anagrams = iterator->second;
			string line = "\t[";
			for (string anagram : anagrams) {
				line += anagram + ", ";
			}
			line = line.substr(0, line.size() - 2) + "]";
			cout << line << endl;
		}
	}
}