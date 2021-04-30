#pragma once

#include "halleystring.h"
#include <vector>
#include <optional>
#include <gsl/span>

namespace Halley {
    class FuzzyTextMatcher {
    public:
		class Score {
		public:
			int curJumps = 0;
			int jumpLength = 0;
			int sections = 0;
			int sectionLens = 0;
			int sectionPos = 0xFFFF;
			std::vector<std::pair<uint16_t, uint16_t>> matchPositions;

			bool operator<(const Score& other) const;

			Score advance(int jumpLen, int sectionPos, int newSectionLen) const;
			void makeMatchPositions(const std::vector<int>& breadcrumbs);
		};
    	
        class Result {
        public:
        	Result() = default;
            Result(String str, String id, Score score);

            bool operator<(const Result& other) const;

        	const String& getString() const;
        	const String& getId() const;
        	gsl::span<const std::pair<uint16_t, uint16_t>> getMatchPositions() const;

        private:
        	String str;
        	String id;
        	std::vector<std::pair<uint16_t, uint16_t>> matchPositions;
        	Score score;
        };

        FuzzyTextMatcher(bool caseSensitive, std::optional<size_t> resultsLimit);
    	
    	void addStrings(std::vector<String> strings);
    	void addString(String string, String id = "");
    	void clear();

    	std::vector<Result> match(const String& query) const;

    private:
        struct Entry {
	        String string;
            String id;
        };
    	
    	std::vector<Entry> strings;
    	bool caseSensitive;
    	std::optional<size_t> resultsLimit;

    	std::optional<Result> match(const String& str, const String& id, const StringUTF32& query) const;
    };
}
