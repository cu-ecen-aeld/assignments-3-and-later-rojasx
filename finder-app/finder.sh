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
echo "Using filesdir: $filesdir"
echo "Using searchstr: $searchstr"