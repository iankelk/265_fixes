# Workload generator and evaluate.py bug fixes and improvements#
---
*This repository contains fixes for the code of a workload generator and workload evaluator for an LSM tree.*

*More information can be found [here](http://daslab.seas.harvard.edu/classes/cs265/project.html).*

## Workload and Data Generator ##
---

## The files that have been modified are `generator.c` and `evaluate.py`

To view the diffs of the commits to see all bug fixes and improvements:

* [generator.c](https://github.com/iankelk/265_fixes/compare/bdd8a48..a6b8414#diff-251a3fd6dc66b5af84694550202afdee564ef49879189e0479ae59a5ec1126fb)
* [evaluate.py](https://github.com/iankelk/265_fixes/compare/bdd8a48..a6b8414#diff-9b330cc5f884f89be6a567df8a3dce69b84eb873eaeb37dbe079f9ab263a0932)

## generator.c ##

#### Issues: ####

The generator.c from the [original repo](https://bitbucket.org/HarvardDASlab/cs265-sysproj/src/master/) has several problems:

1. The `--seed` argument doesn't work, which means the workloads generated are identical. This is a problem if you need different workloads for testing concurrent clients.

2. It front-loads everything. If you do 1,000,000 PUTs and 100 GETs, those 100 GETs will be mixed in with the first 100 PUTs, and then ... nothing but 9,999,800 PUTs after that.

3. The `gets-misses-ratio` option is always off by 10%-20%. The default behavior was supposed to be 0.5, but in the example below it actually achieves 323 misses to 677 gets, which would be a gets-misses ratio of 677/(323+677) = 0.677 or roughly 0.7. You can confirm this with the updated version of `evaluate.py`

Here is an example of generator output using the original `generator.c` without fixes:

```sh
+---------- CS 265 ----------+
| WORKLOAD INFO              |
+----------------------------+
| initial-seed: 13141
| puts-total: 10000
| gets-total: 1000
| get-skewness: 0.0000
| ranges: 1000
| range distribution: uniform
| deletes: 100
| gets-misses-ratio: 0.5000
+----------------------------+
```

And the output from `evaluate.py` which shows the `get-misses-ratio` is *actually* `0.677` and not `0.5`

```sh
cs265 % python evaluate.py workload
------------------------------------
PUTS 10000
SUCCESFUL_GETS 323
FAILED_GETS 677
RANGES 1000
SUCCESSFUL_DELS 49
FAILED_DELS 51
LOADS 0
TIME_ELAPSED 0.05323910713195801
------------------------------------
```

4. The RANGE generation is very problematic. It generates 2 keys within the entire space of `KEY_MAX 2147483647` to `KEY_MIN -2147483647`. As a result, in a database with a billion entries, the RANGE queries would start returning ridiculously large ranges that could theoretically reach a billion entries (in practice it was still in the millions). Even when I ran 1 million PUTs and 100 RANGEs and then shuffled it to avoid the front-loading mentioned in problem 2 above, it would return a total of almost 18 million entries since each of the 100 ranges was MASSIVE.

While it's good to be able to test for such situations with ranges in the millions, it's much better to have some control over this behaviour and be able to tweak the workload generator so that it can generate smaller ranges as well, despite having a very large number of entries in the database.

#### Fixes: ####

1. `--seed` has been fixed and now generates different workloads based on the seed.

2. The code now calculates the remaining operations for each type and then calculates the percentage relative to the total remaining number of commands for each operation. Now, the PUT, GET, DELETE, and RANGE operations are not clustered around the beginning and are evenly distributed throughout the workload.

3. I've corrected the error that was putting it off by 10% by performing integer division on the random number with RAND_MAX instead of taking modulo 10. This fixed the issue. Specifically: 

```c
// This
if((rand()%10) > (s->gets_skewness*10) || old_gets_pool_count == 0) {
// Was changed to this:
if((((float)rand()) / RAND_MAX > s->gets_skewness) || old_gets_pool_count == 0) {
```

4. To fix the creation of huge ranges, I've introduced another argument `--max-range-size`. Now you can set `--max-range-size` as one of the arguments. This will take whatever value you have, get a random number using a uniform distribution for it between its negative and positive values, and add it to the beginning of the range. For example, if you set `max-range-size` to be `1000` and the start key picked was `1674510637`, it will uniformly pick a range size from `-1000` to `1000` and add it to `1674510637`. It might pick 313 and print the range `r 1674510637 1674510950`. This allows you to test RANGE queries without having your database brought to a crawl by it repeatedly trying to return ranges of millions of key-value pairs. It also ensures that the ranges do not go beyond the bounds of `KEY_MIN` and `KEY_MAX`.

## Here is a screenshot of the modified help screen which includes the `--max-range-size` option.

### Help screenshot
![Screen shot of generator help](./img/generator_help.jpg)

### Example ###
**Query:** Insert 100000 keys, perform 1000 gets and 10 range queries (with a maximum range size of 100000 entries) and 20 deletes. The amount of misses of gets should be approximately 30% (`--gets-misses-ratio`) and 20% of the queries should be repeated (`--gets-skewness`).

```
./generator --puts 100000 --gets 1000 --ranges 10 --deletes 20 --gets-misses-ratio 0.3 --gets-skewness 0.2 --max-range-size 100000 > workload.txt
```

### Example of results of generating workload
![Screen shot of generator results](./img/generator_results.jpg)

## evaluate.py ##

## Evaluating a Workload ##
---
You can execute a workload and see some basic statistics about it, using the `evaluate.py` python script.

### Dependencies ###
You need to install the [sortedcontainers](https://pypi.org/project/sortedcontainers/) library.

Most platforms: ```pip install sortedcontainers```

*Note: In Fedora Linux, you might need to install it using: ```dnf install python-sortedcontainers```.*

*Note: In previous versions, `evaluate.py` used a library called `blist`, however that library has not been maintained since 2014 and is not compatible with Python 3.*

#### Issues: ####

1. The `blist` library hasn't been updated since 2014 and is not compatible with modern python. As well, all the print statements are written in Python 2.

2. Inside the main() function, there are two flags, `verbose` and `show_output` which are hardcoded and not explained.

#### Fixes: ####

1. I've corrected these problems. I've replaced the `blist` library with the `sortedcontainers` library, which is an actively maintained alternative providing sorted list and sorted dictionary data structures. You can install it using:

`pip install sortedcontainers`

2. The two flags, `verbose` and `show_output` have been changed to command-line arguments. To do this, `import argparse` was added to the top of the file, but it does not require any additional libraries to be installed.

#### Improvements: ####

The evaluation report now also displayes the number of non-empty ranges found, as well as the smallest and largest ranges (by number of entries) found.

### Running ###

Run as follows:
```
python evaluate.py [-h] [-v] [-s] [workload.txt]
```

### Help screenshot
![Screen shot of evaluator help](./img/evaluate_help.jpg)

### Example of results of evaluating workload generated above
![Screen shot of evaluator results](./img/evaluate_results.jpg)
