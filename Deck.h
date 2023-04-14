#pragma once
#include <stdlib.h>
#include <time.h>
#include <numeric>
#include <queue>

class Deck {
public:
	static std::queue<int> GetDeck() {
		int arr[52];
		std::queue<int> q;
		std::iota(arr, arr + 52, 1);
		srand(time(NULL));
		for (int i = 51; i > 0; i--)
		{
			int j = rand() % (i + 1);
			std::swap(arr[i], arr[j]);
		}
		for (int i = 0; i < 52; i++) {
			q.push(arr[i]);
		}
		return q;
	}

	static void SetCardValues(int* cardValues) {
		int num = 0;
		// Loop to assign values to the cards
		for (int i = 0; i < 53; i++) {
			num = i;
			// Count up to the amount of cards, 52
			num %= 13;
			if (num > 10 || num == 0) {
				num = 10;
			}
			cardValues[i] = num++;
		}
	}


};