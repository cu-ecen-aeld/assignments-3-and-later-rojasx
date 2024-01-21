#!/bin/bash
# Will use this multiple times
help_statement="Usage: finder.sh <filesdir> <searchstr>"

# Check if there's any arguments at all
if [ "$#" == 0 ]; then
    echo -e "error: no arguments.\n$help_statement"
    exit 1
# Check if there's no second argument
elif [ "$2" == "" ]; then
    echo -e "error: bad arguments.\n$help_statement"
    exit 1
# Check if there filesdir exists
elif [ ! -d "$1" ]; then
    echo -e "error: <filesdir> provided does not exist or is not a dir.\n$help_statement"
    exit 1
fi

# Filepath variable, first argument
filesdir="$1"
searchstr="$2"

search_output=$(find "$filesdir" -type f -exec grep "$searchstr" {} +)

# Count the number of matching files (x)
trimmed=$(cut -d: -f1 <<< "$search_output")
sorted=$(sort -u <<< "$trimmed")
x=$(wc -l <<< "$sorted")
# x=$(echo "$search_output" | cut -d: -f1 | sort -u | wc -l)

# Count the total number of matching lines (y)
y=$(wc -l <<< "$search_output")
# y=$(echo "$search_output" | wc -l)

# # Output the results
echo "The number of files are $x and the number of matching lines are $y"