#include <algorithm>
#include <sstream>
#include <iterator>

#include "Grammar.h"
#include "Utilities.h"


using namespace std;


bool operator<(const Item& first, const Item& second)
{
	return first.ProductionHead < second.ProductionHead &&
		first.RuleIndex < second.RuleIndex &&
		first.DotIndex < second.RuleIndex;
}


inline const string EPSILON()
{
	const std::string epsilon = "EPSILON";
	return epsilon;
}

inline const string ENDMARKER()
{
	const std::string endmarker = "ENDMARKER";
	return endmarker;
}

inline const string AUGMENTED_START()
{
	const std::string endmarker = "AUGMENTED_START";
	return endmarker;
}


Grammar::Grammar(string start, map<string, RuleList> rules, bool shouldAugment)
{
	this->startSymbol = start;
	this->rules = rules;

	set<string> symbols;

	for (auto& pair : this->rules)
	{
		this->nonterminals.insert(pair.first);
		symbols.insert(pair.first);

		for (auto& rhs : pair.second)
		{
			for (auto& rhsSymbol : rhs)
			{
				symbols.insert(rhsSymbol);
			}
		}
	}
	set_difference(symbols.begin(), symbols.end(),
		this->nonterminals.begin(), this->nonterminals.end(),
		inserter(this->terminals, terminals.begin()));


	if (shouldAugment)
	{
		/*this->rules.insert(this->rules.begin(), { AUGMENTED_START(), { AUGMENTED_START(), this->startSymbol } });*/
		auto& augmented = AUGMENTED_START();
		vector<string> rule = { this->startSymbol };
		this->rules[augmented] = { rule };
		this->nonterminals.insert(augmented);
		this->terminals.insert(ENDMARKER());
		this->startSymbol = augmented;
	}

	this->ComputeFirst();
	this->ComputeFollow();
}


int Grammar::ComputeFirstStep(string symbol)
{
	RuleList& symbolRules = this->rules[symbol];
	if (this->first.find(symbol) == this->first.end())
	{
		auto equalsEpsilon = [&symbol](const vector<string>& rhs) -> bool {
			return rhs.size() == 1 && rhs[0] == EPSILON();
		};
		this->first[symbol] = set<string>();
		if (any_of(symbolRules.begin(), symbolRules.end(), equalsEpsilon))
		{
			this->first[symbol].insert(EPSILON());
		}
		return this->first[symbol].size();
	}
	int currentSize = this->first[symbol].size();
	for (auto& rhs : symbolRules)
	{
		int index = 0;
		string current = rhs[index];
		this->first[symbol].insert(this->first[current].begin(), first[current].end());

		while (first[current].find(EPSILON()) != first[current].end() && index < rhs.size() - 1)
		{
			string& next = rhs[index++];
			first[symbol].insert(first[next].begin(), first[next].end());
			current = next;
		}
		if (index == rhs.size() - 1 && first[current].find(EPSILON()) != first[current].end())
			first[symbol].insert(first[current].begin(), first[current].end());
		else
		{
			// If we are here, some symbol on the rhs doesn't derive Epsilon, thus remove Epsilon if
			// it has been added by another symbol
			auto iter = first[symbol].find(EPSILON());
			if (iter != first[symbol].end())
				first[symbol].erase(iter);
		}
	}
	return first[symbol].size() - currentSize;
}

void Grammar::ComputeFirst()
{
	for (auto& terminal : this->terminals)
	{
		this->first[terminal] = { terminal };
	}

	int added = 0;
	do
	{
		added = 0;
		for (auto& nonterminal : this->nonterminals)
		{
			added += this->ComputeFirstStep(nonterminal);
		}
	} while (added != 0);
}

ostream& operator<<(ostream& stream, const Grammar& g)
{
	stream << "FIRST: " << endl;
	for (auto pair : g.first)
	{
		stream << pair.first << " -> ";
		JoinCollection(pair.second, stream) << endl;
	}

	stream << "FOLLOW: " << endl;
	for (auto pair : g.follow)
	{
		stream << pair.first << " -> ";
		JoinCollection(pair.second, stream) << endl;
	}

	return stream;
}


set<string> Grammar::ComputeFirstWord(const vector<string>& word)
{
	if (word.size() == 0) return set<string>();

	set<string> wordFirst;
	int index = 0;
	string current = word[index];
	wordFirst.insert(this->first[current].begin(), first[current].end());

	while (this->first[current].find(EPSILON()) != this->first[current].end() && index < word.size() - 1)
	{
		const string& next = word[index++];
		wordFirst.insert(this->first[next].begin(), this->first[next].end());
		current = next;
	}
	if (index == word.size() - 1 && this->first[current].find(EPSILON()) != this->first[current].end())
		wordFirst.insert(this->first[current].begin(), this->first[current].end());
	else
	{
		// If we are here, some symbol on the rhs doesn't derive Epsilon, thus remove Epsilon if
		// it has been added by another symbol
		auto iter = wordFirst.find(EPSILON());
		if (iter != wordFirst.end())
			wordFirst.erase(iter);
	}
	return wordFirst;
}


int Grammar::ComputeFollowStep(std::string symbol)
{
	RuleList& symbolRules = this->rules[symbol];
	if (this->follow.find(symbol) == this->follow.end())
	{
		this->follow[symbol] = set<string>();
		return 1;
	}
	int added = 0;
	for (auto& rhs : symbolRules)
	{
		for (auto it = rhs.begin(); it != rhs.end(); ++it)
		{
			if (this->nonterminals.find(*it) != this->nonterminals.end())
			{
				int currentSize = this->follow[*it].size();
				bool isLast = it + 1 == rhs.end();
				vector<string> beta(it + 1, rhs.end());
				auto firstBeta = this->ComputeFirstWord(beta);
				// Case 2
				if (!isLast)
				{
					this->follow[*it].insert(firstBeta.begin(), firstBeta.end());
				}

				// Case 3
				bool followedByEpsilon = firstBeta.find(EPSILON()) != firstBeta.end();
				if (isLast || followedByEpsilon)
				{
					this->follow[*it].insert(this->follow[symbol].begin(), this->follow[symbol].end());
				}
				added += this->follow[*it].size() - currentSize;
			}
		}
	}
	return added;
}

void Grammar::ComputeFollow()
{
	this->follow[this->startSymbol] = { ENDMARKER() };
	set<string> epsilonSet = { EPSILON() };

	int added = 0;
	do
	{
		added = 0;
		for (auto& nonterminal : this->nonterminals)
		{
			added += this->ComputeFollowStep(nonterminal);
		}
	} while (added != 0);

	// remove all epsilons
	for (auto& pair : this->follow)
	{
		auto iter = pair.second.find(EPSILON());
		if (iter != pair.second.end())
			pair.second.erase(iter);
	}
}


set<Item>& Grammar::Closure(set<Item>& setOfItems)
{
	// See page 261
	int added = 0;
	do
	{
		int currentSize = setOfItems.size();
		for (auto& item : setOfItems)
		{
			RuleBody& body = this->rules[item.ProductionHead][item.RuleIndex];
			const string& nonterminal = body[item.DotIndex];
			if (this->nonterminals.find(nonterminal) == this->nonterminals.end())
			{
				continue;
			}

			RuleBody tail(body.begin() + item.DotIndex + 1, body.end());
			tail.push_back(item.Lookahead);
			auto firstTail = this->ComputeFirstWord(tail);
			auto& bodies = this->rules[nonterminal];
			for (auto it = bodies.begin(); it != bodies.end(); it++)
			{
				for (auto& terminal : firstTail)
				{
					Item newItem;
					newItem.ProductionHead = nonterminal;
					newItem.Lookahead = terminal;
					newItem.RuleIndex = distance(it, bodies.begin());
					newItem.DotIndex = 0;
					setOfItems.insert(newItem);/*t,jgrtjhrthgrtj*/
				}
			}
		}
		added += setOfItems.size() - currentSize;

	} while (added != 0);
	return setOfItems;
}

set<Item> GoTo(const std::set<Item>& setOfItems, const std::string& terminal)
{
	return set<Item>();
}
void ComputeItems();