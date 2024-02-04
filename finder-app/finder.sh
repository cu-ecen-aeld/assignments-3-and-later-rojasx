#!/bin/sh
# Will use this multiple times
help_statement="Usage: finder.sh <filesdir> <searchstr>"

# Check if there are any arguments at all
if [ "$#" = 0 ]; then
    echo -e "error: no arguments.\n$help_statement"
    exit 1
# Check if there's no second argument
elif [ "$2" = "" ]; then
    echo -e "error: bad arguments.\n$help_statement"
    exit 1
# Check if the filesdir exists
elif [ ! -d "$1" ]; then
    echo -e "error: <filesdir> provided does not exist or is not a dir.\n$help_statement"
    exit 1
fi

# Filepath variable, first argument
filesdir="$1"
searchstr="$2"

search_output=$(find "$filesdir" -type f -exec grep "$searchstr" {} +)

# Count the number of files in the dir
x=$(find "$filesdir" -type f | wc -l)

# Count the total number of matching lines (y)
y=$(echo "$search_output" | wc -l)

# Output the results
echo "The number of files are $x and the number of matching lines are $y"
