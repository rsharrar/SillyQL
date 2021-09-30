// Project Identifier: 0E04A31E0D60C01986ACB20081C9D8722A1899B6
#include <vector>
#include <iostream>
#include <algorithm>
#include <getopt.h>
#include <string>
#include <deque>
#include <queue>
#include <cassert>
#include <sstream>
#include "P2random.h"
#include <limits>

using namespace std;
class Market {
private:
	struct Order {
		bool buy = false;
		bool sell = false;
		int t_stamp = 99999;
		int trader_id = 99999;
		int stock_id = 99999;
		int price_limit = 99999;
		mutable int quantity = 99999;
		int order_num = 0;
	};
	
	struct Trader{
		int bought = 0;
		int sold = 0;
		int net_transfer = 0;
	};

	struct Traveler {
		bool have_stock = false;
		int sell_price = 0;
		int buy_price = 0;
		int possible_sell_price = 0;
		int sell_t_stamp = -1;
		int buy_t_stamp = -1;
		int possible_sell_t_stamp = -1;
	};

	struct Stock {
		priority_queue <int, vector<int>, greater<int>> max_queue;
		priority_queue <int, vector<int>, less<int>>  min_queue;
		int median = 0;
		bool traded = false;
	};

	struct BuyCompare
	{
		bool operator () (Order*& order1, Order*& order2) const {
			if (order1->price_limit < order2->price_limit) {
				return true;
			}
			else if (order2->price_limit < order1->price_limit) {
				return false;
			}
			else {
				if (order1->t_stamp < order2->t_stamp) {
					return false;
				}
				else if (order2->t_stamp < order1->t_stamp) {
					return true;
				}
				else {
					if (order1->order_num < order2->order_num) {
						return false;
					}
					else if (order2->order_num < order1->order_num) {
						return true;
					}
				}
			}
			assert(false);
			return false;
		}

	};

	struct SellCompare
	{
		bool operator () (Order*& order1, Order*& order2) const {
			if (order2->price_limit < order1->price_limit) {
				return true;
			}
			else if (order1->price_limit < order2->price_limit) {
				return false;
			}
			else {
				if (order1->t_stamp < order2->t_stamp) {
					return false;
				}
				else if (order2->t_stamp < order1->t_stamp) {
					return true;
				}
				else {
					if (order1->order_num < order2->order_num) {
						return false;
					}
					else if (order2->order_num < order1->order_num) {
						return true;
					}
				}
			}
			assert(false);
			return false;
		}
	};

	void errorOut() {
		cerr << "Input invalid. Make sure that timestamps are non-negative, "
			<< "trader ID's and stock ID's are less than or equal to the "
			<< "total number of traders or stocks respectively, price and quantity values are positive"
			<< " (non-zero), and that timestamps are non-decreasing.\n";
		exit(1);
	}

	void pushMedian(const int& price, const int& stock) {
		auto& max_queue = stocks[stock].max_queue;
		auto& min_queue = stocks[stock].min_queue;

		if (price >= max_queue.top()) max_queue.push(price);
		else min_queue.push(price);

		if ((max_queue.size() - min_queue.size()) == 2) {
			min_queue.push(max_queue.top());
			max_queue.pop();
		}
		else if ((min_queue.size() - max_queue.size()) == 2) {
			max_queue.push(min_queue.top());
			min_queue.pop();
		}
	}

	//buy = order1, sell = order2
	bool attemptTrade(Order* buy_order, Order* sell_order, bool current_order_is_buy) {
		if ((buy_order->quantity == 0) || (sell_order->quantity == 0)) return false;
		if (sell_order->price_limit > buy_order->price_limit) return false;
		int price = 0;
		if (buy_order->t_stamp < sell_order->t_stamp) { price = buy_order->price_limit; }
		else if (sell_order->t_stamp < buy_order->t_stamp) { price = sell_order->price_limit; }
		else if (current_order_is_buy) { price = sell_order->price_limit; }
		else { price = buy_order->price_limit; }
		if ((buy_order->stock_id == sell_order->stock_id)
			&& (sell_order->price_limit <= buy_order->price_limit)) {
			int quantity = min(buy_order->quantity, sell_order->quantity);
			if (verbose_option) {
				cout << "Trader " << buy_order->trader_id << " purchased " << quantity
					<< " shares of Stock " << buy_order->stock_id << " from Trader "
					<< sell_order->trader_id << " for $" << price << "/share\n";
			}
			if (info_option) {
				all_traders_vector[buy_order->trader_id].bought += quantity;
				all_traders_vector[buy_order->trader_id].net_transfer += -(quantity*price);
				all_traders_vector[sell_order->trader_id].sold += quantity;
				all_traders_vector[sell_order->trader_id].net_transfer += (quantity * price);
			}
			if (median_option) {
				if (!stocks[sell_order->stock_id].traded) stocks[sell_order->stock_id].traded = true;
				pushMedian(price,sell_order->stock_id);
			}
			++orders_processed;
			return true;
		}
		return false;
	}
	void processBuyOrder(Order*& buy_order) {
		Order* sell_order = nullptr;
		int ID = buy_order->stock_id;
		while (!SellQueueVec[ID].empty()) {
			sell_order = SellQueueVec[ID].top();
			if (buy_order->price_limit < sell_order->price_limit) break;
			SellQueueVec[ID].pop();
			if (attemptTrade(buy_order, sell_order, true)) {
				if (buy_order->quantity < sell_order->quantity) {
					sell_order->quantity = (sell_order->quantity - buy_order->quantity);
					buy_order->quantity = 0;
				}
				else if (sell_order->quantity < buy_order->quantity) {
					buy_order->quantity = (buy_order->quantity - sell_order->quantity);
					sell_order->quantity = 0;
				}
			}
			if (sell_order->quantity != 0) {
				ptr_dump.push_back(sell_order);
			}
		}
		if (buy_order->quantity != 0) BuyQueueVec[ID].push(buy_order);
		while (!ptr_dump.empty()) {
			SellQueueVec[ID].push(ptr_dump.back());
			ptr_dump.pop_back();
		}
	}
	void processSellOrder(Order * &sell_order) {
		Order* buy_order = nullptr;
		int ID = sell_order->stock_id;
		while (!BuyQueueVec[ID].empty()) {
			buy_order = BuyQueueVec[ID].top();
			if (buy_order->price_limit < sell_order->price_limit) break;
			BuyQueueVec[ID].pop();
			if (attemptTrade(buy_order, sell_order, false)) {
				if (sell_order->quantity < buy_order->quantity) {
					buy_order->quantity = (buy_order->quantity - sell_order->quantity);
					sell_order->quantity = 0;
				}
				else if (buy_order->quantity < sell_order->quantity) {
					sell_order->quantity = (sell_order->quantity - buy_order->quantity);
					buy_order->quantity = 0;
				}
			}
			if (buy_order->quantity != 0) {
				ptr_dump.push_back(buy_order);
			}
		}
		if (sell_order->quantity != 0) SellQueueVec[ID].push(sell_order);
		while (!ptr_dump.empty()) {
			BuyQueueVec[ID].push(ptr_dump.back());
			ptr_dump.pop_back();
		}
	}

	void simulate(Order* &current_order) {
		//cout << "current: ";
		//printOrder(current_order);
		if ((current_order->t_stamp != current_timestamp)) {
			if (median_option) printMedian();
			current_timestamp = current_order->t_stamp;
		}
		//current order is buy
		if ((current_order->buy)) {
			processBuyOrder(current_order);
		}
		//change to current order = sell
		else{
			processSellOrder(current_order);
		}
	}

	void calcMedian(Stock& stock) {
		auto& max_queue = stock.max_queue;
		auto& min_queue = stock.min_queue;
		if (min_queue.size() == max_queue.size()) stock.median = ((min_queue.top() + max_queue.top()) / 2);
		else if (max_queue.size() > min_queue.size()) stock.median = max_queue.top();
		else stock.median = min_queue.top();
	}

	void printMedian() {
		for (size_t i = 0; i < stocks.size(); ++i) {
			Stock &current_stock = stocks[i];
			if (current_stock.traded) {
				calcMedian(current_stock);
				cout << "Median match price of Stock " << i << " at time "
					<< current_timestamp << " is $" << current_stock.median
					<< "\n";
			}
		}
	}

	void timeTravelerSim(Order* &order) {
		if (order->sell) {
			if (!travelers[order->stock_id].have_stock
				|| (travelers[order->stock_id].possible_sell_price > order->price_limit)) {
				travelers[order->stock_id].have_stock = true;
				travelers[order->stock_id].possible_sell_price = order->price_limit;
				travelers[order->stock_id].possible_sell_t_stamp = order->t_stamp;
			}
		}
		if (order->buy && travelers[order->stock_id].have_stock) {
			if ((order->price_limit > travelers[order->stock_id].buy_price)
				|| ((order->price_limit - travelers[order->stock_id].possible_sell_price)
				> (travelers[order->stock_id].buy_price - travelers[order->stock_id].sell_price))) {
				travelers[order->stock_id].buy_price = order->price_limit;
				travelers[order->stock_id].buy_t_stamp = order->t_stamp;
				travelers[order->stock_id].sell_price
					= travelers[order->stock_id].possible_sell_price;
				travelers[order->stock_id].sell_t_stamp
					= travelers[order->stock_id].possible_sell_t_stamp;
			}
		}
	}

	void makeOrders() {
		istream& inputstream = (pr_mode) ? ss : cin;
		unsigned int in1;
		bool first_order = true;
		string in2;
		string in3;
		string in4;
		string in5;
		string in6;
		cout << "Processing orders...\n";
		while (inputstream >> in1 >> in2 >> in3 >> in4
			>> in5 >> in6) {
			Order* order = new Order;
			order->t_stamp = in1;
			if (first_order) {
				current_timestamp = in1;
				first_order = false;
			}
			if (in2 == "BUY") order->buy = true;
			else if (in2 == "SELL") order->sell = true;
			in3.erase(in3.begin());
			order->trader_id = stoi(in3);
			in4.erase(in4.begin());
			order->stock_id = stoi(in4);
			in5.erase(in5.begin());
			order->price_limit = stoi(in5);
			in6.erase(in6.begin());
			order->quantity = stoi(in6);
			all_orders_vector.push_back(order);
			order->order_num = order_count;
			order_count++;
			if ((order->t_stamp < 0) || (order->trader_id >= num_traders) 
				|| (order->stock_id >= num_stocks) || (order->price_limit <= 0)
				|| (order->quantity <= 0) || (order->t_stamp < previous_timestamp)){
				cleanup();
				errorOut();
			}
			//printOrder(all_orders_vector.back());
			previous_timestamp = order->t_stamp;
			if (time_traveler_option) timeTravelerSim(order);
			simulate(order);
		}
		if (median_option) printMedian();
		cout << "---End of Day---\n" << "Orders Processed: " << orders_processed << "\n";
		if (info_option) printTraderInfo();
		if (time_traveler_option) printTravelerInfo();
		return;
	}

	void printTraderInfo() {
		cout << "---Trader Info---\n";
		for (size_t i = 0; i < all_traders_vector.size(); ++i) {
			cout << "Trader " << i << " bought " << all_traders_vector[i].bought
				<< " and sold " << all_traders_vector[i].sold << " for a net transfer of $"
				<< all_traders_vector[i].net_transfer << "\n";
		}
	}

	void printTravelerInfo() {
		cout << "---Time Travelers---\n";
		for (size_t i = 0; i < travelers.size(); ++i) {
			cout << "A time traveler would buy shares of Stock " << i << " at time: "
				<< travelers[i].sell_t_stamp << " and sell these shares at time: "
				<< travelers[i].buy_t_stamp << "\n";
		}
	}

	void printOrder(Order* &order) {
		cout << order->t_stamp << " ";
		if (order->buy) cout << "BUY";
		else if (order->sell) cout << "SELL";
		cout << " T" << order->trader_id << " S" << order->stock_id
			<< " $" << order->price_limit << " #" << order->quantity << "\n";
	}

	stringstream ss;
	vector<priority_queue<Order*, vector<Order*>, BuyCompare>> BuyQueueVec;
	vector<priority_queue<Order*, vector<Order*>, SellCompare>> SellQueueVec;
	vector<Order*> ptr_dump;
	vector<Order*> all_orders_vector;
	vector<Trader> all_traders_vector;
	vector<Stock> stocks;
	vector<Traveler> travelers;
	int order_count = 0;
	int num_traders = 0;
	int num_stocks = 0;
	unsigned int random_seed = 0;
	unsigned int num_orders = 0;
	unsigned int arrival_rate = 0;
	unsigned int orders_processed = 0;
	int current_timestamp = 0;
	int previous_timestamp = 0;
	bool verbose_option = false;
	bool median_option = false;
	bool info_option = false;
	bool time_traveler_option = false;
	bool tl_mode = false;
	bool pr_mode = false;
public:
	void get_options(int argc, char** argv) {
		int option_index = 0, option = 0;

		opterr = false;
		struct option longOpts[] = { { "verbose", no_argument, nullptr, 'v' },
									{ "median", no_argument, nullptr, 'm' },
									{ "trader_info", no_argument, nullptr, 'i'},
									{ "time_travelers", no_argument, nullptr, 't'},
									{ "length", no_argument, nullptr, 'l'},
									{ nullptr, 0, nullptr, '\0' } };

		while ((option = getopt_long(argc, argv, "vmit", longOpts, &option_index)) != -1) {
			switch (option) {
			case 'v':
				verbose_option = true;
				break;
			case 'm':
				median_option = true;
				break;
			case 'i':
				info_option = true;
				break;
			case 't':
				time_traveler_option = true;
				break;
			}
		}
	}

	void RunSimulation() {
		string mode;
		string junk;
		getline(cin, junk);
		cin >> junk;
		cin >> mode;
		if (mode == "TL") tl_mode = true;
		else if (mode == "PR") pr_mode = true;
		cin >> junk;
		cin >> num_traders;
		cin >> junk;
		cin >> num_stocks;
		if (pr_mode) {
			cin >> junk;
			cin >> random_seed;
			cin >> junk;
			cin >> num_orders;
			cin >> junk;
			cin >> arrival_rate;
			P2random::PR_init(ss, random_seed, num_traders, num_stocks, num_orders, arrival_rate);
		}
		BuyQueueVec.reserve(num_stocks);
		SellQueueVec.reserve(num_stocks);
		priority_queue<Order*,vector<Order*>, BuyCompare> b;
		priority_queue<Order*, vector<Order*>, SellCompare> s;
		for (int i = 0; i < num_stocks; ++i) {
			BuyQueueVec.push_back(b);
			SellQueueVec.push_back(s);
		}
		if (info_option) {
			all_traders_vector.reserve(num_traders);
			Trader t;
			for (int i = 0; i < num_traders; ++i) {
				all_traders_vector.push_back(t);
			}
		}
		if (time_traveler_option) {
			travelers.reserve(num_stocks);
			Traveler t;
			for (int i = 0; i < num_stocks; ++i) {
				travelers.push_back(t);
			}
		}
		if (median_option) {
			stocks.reserve(num_stocks);
			Stock s;
			for (int i = 0; i < num_stocks; ++i) {
				s.max_queue.push(numeric_limits<int>::max());
				s.min_queue.push(numeric_limits<int>::min());
				stocks.push_back(s);
			}
		}
		makeOrders();
	}

	void cleanup() {
		while (!all_orders_vector.empty()) {
			delete (all_orders_vector.back());
			all_orders_vector.back() = nullptr;
			all_orders_vector.pop_back();
		}
	}

	void test_print() {
		cout << verbose_option << "\n";
		cout << median_option << "\n";
		cout << info_option << "\n";
		cout << time_traveler_option << "\n";
	}
};

int main(int argc, char** argv) {
	Market myMarket;

	myMarket.get_options(argc,argv);

	myMarket.RunSimulation();

	myMarket.cleanup();
}