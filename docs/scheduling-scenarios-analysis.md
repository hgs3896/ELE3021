# Scheduling Scenarios Analysis

## 1. 10 MLFQ processes

![Workload 0](/docs/project1/workload0.png)

|name|   type|  cnt|measured percent|  z score|
| -  |   -   |  -  |        -       |    -    |
|MLfQ|compute|90572|        9.251991|-1.809686|
|MLfQ|compute|93852|        9.587046|-0.999076|
|MLfQ|compute|94440|        9.647110|-0.853760|
|MLfQ|compute|96800|        9.888186|-0.270516|
|MLfQ|compute|97675|        9.977568|-0.054271|
|MLfQ|compute|98599|       10.071955| 0.174083|
|MLfQ|compute|100714|       10.288004| 0.696778|
|MLfQ|compute|101190|       10.336627| 0.814416|
|MLfQ|compute|101679|       10.386579| 0.935266|
|MLfQ|compute|103425|       10.564934|  1.36676|

Since MLFQ processes should be given a cpu equally, the expectation matches the results.

## 2. 5 MLFQ processes counting levels

![Workload 1](/docs/project1/workload1.png)

|name|     type|  cnt| lev0| lev1| lev2|measured percent|z score|
| -  |    -    |  -  |  -  |  -  |  -  |       -        |-|
|MLfQ|  compute|87375|11196|23006|53173|       19.630817|-1.671233|
|MLfQ|  compute|89078|11941|23464|53673|       20.013435| 0.060820|
|MLfQ|  compute|89112|11947|23482|53683|       20.021074| 0.095400|
|MLfQ|  compute|89613|12547|23436|53630|       20.133636| 0.604947|
|MLfQ|  compute|89913|12829|23468|53616|       20.201038| 0.910065|

The ratio of lev0 and lev1 is almost 1:2, since the lev0 has 5 ticks quantum and lev1 has 10ticks quantum.

## 3. 10 MLFQ processes yielding repeatedly

![Workload 2](/docs/project1/workload2.png)

|name|type|cnt|measured percent|z score|
|-|-|-|-|-|
|MLfQ|yield|20672|9.995552|-2.025910|
|MLfQ|yield|20677|9.997969|-0.924872|
|MLfQ|yield|20678|9.998453|-0.704664|
|MLfQ|yield|20681|9.999903|-0.044042|
|MLfQ|yield|20681|9.999903|-0.044042|
|MLfQ|yield|20683|10.000870|0.396374|
|MLfQ|yield|20684|10.001354|0.616581|
|MLfQ|yield|20684|10.001354|0.616581|
|MLfQ|yield|20684|10.001354|0.616581|
|MLfQ|yield|20688|10.003288|1.497412|

This is similar as when 10 MLFQ process are running, but it has lesser counts due to yielding.

## 4. 5 MLFQ processes not only yielding repeatedly, but also counting levels

![Workload 3](/docs/project1/workload3.png)

|name| type|  cnt|lev0|lev1| lev2|measured percent|  z score|
| -  |  -  | -   |  - | -  |  -  |    -           |      -  |
|MLfQ|yield|33123|  45|  90|32988|       19.993602|-1.451919|
|MLfQ|yield|33132|  45|  90|32997|       19.999034|-0.219158|
|MLfQ|yield|33132|  45|  90|32997|       19.999034|-0.219158|
|MLfQ|yield|33140|  45|  90|33005|       20.003863| 0.876630|
|MLfQ|yield|33141|  45|  90|33006|       20.004467| 1.013604|

It is a lock consuming the overall performance which both yield syscall and `getlev` have.
In addition, yield syscall has an effect to MLFQ's consumed tick counts, so this accelerates the MLFQ scheduler to put this process in the lower priority queue.

Compared to workload 2, the processes get smaller ticks, but the ratio stays almost same.

## 5. 5 MLFQ processes and 5 MLFQ processes yielding repeatedly
![Workload 4](/docs/project1/workload4.png)

|name|   type|cnt|measured percent|z score|
|-|-|-|-|-|
|MLfQ|  yield|   169| 0.017397|-0.948330|
|MLfQ|  yield|   174| 0.017912|-0.948282|
|MLfQ|  yield|   174| 0.017912|-0.948282|
|MLfQ|  yield|   202| 0.020794|-0.948008|
|MLfQ|  yield|  1687| 0.173661|-0.933486|
|MLfQ|compute|170868|17.589237| 0.720965|
|MLfQ|compute|197930|20.375012| 0.985609|
|MLfQ|compute|198423|20.425762| 0.990430|
|MLfQ|compute|198972|20.482276| 0.995799|
|MLfQ|compute|202836|20.880038| 1.033585|

## 6. 10 Stride processes(8% share)
![Workload 5](/docs/project1/workload5.png)

|  name|percent|   cnt|measured percent|  z score|
|-|-|-|-|-|
|STRIDE|      8| 94003|        9.445240|-1.162405|
|STRIDE|      8| 94309|        9.475987|-1.097982|
|STRIDE|      8| 95725|        9.618264|-0.799864|
|STRIDE|      8| 96161|        9.662072|-0.708071|
|STRIDE|      8| 97316|        9.778124|-0.464903|
|STRIDE|      8|100801|       10.128290| 0.268811|
|STRIDE|      8|100971|       10.145372| 0.304602|
|STRIDE|      8|103227|       10.372050| 0.779569|
|STRIDE|      8|104999|       10.550097| 1.152637|
|STRIDE|      8|107730|       10.824503| 1.727607|

Every stride process that has 8% share gets an equal share.

## 7. 8 Stride processes(1%, 3%, 5%, 7%, 11%, 13%, 17%, 23% share respectively) and 2 MLFQ processes
![Workload 6](/docs/project1/workload6.png)

|name|percent|
|:-:|:-:|
|MLfQ|20.259749%|
|STRIDE|79.740251%|

|  name|   type|percent|   cnt|measured percent|relative percent|
|   -  |   -   |    -  |  -   |        -       |       -        |
|  MLfQ|compute|      *| 82417|        8.514733|       42.027833|
|  MLfQ|compute|      *|113684|       11.745016|       57.972167|
|STRIDE|       |      1| 14025|        1.448962|        1.817103|
|STRIDE|       |      3| 28182|        2.911562|        3.651308|
|STRIDE|       |      5| 51634|        5.334455|        6.689789|
|STRIDE|       |      7| 69673|        7.198115|        9.026953|
|STRIDE|       |     11|102872|       10.627997|       13.328272|
|STRIDE|       |     13|122065|       12.610880|       15.814950|
|STRIDE|       |     17|168725|       17.431457|       21.860299|
|STRIDE|       |     23|214657|       22.176822|       27.811327|

According to its share, 2 MLFQ processes have 22.134684% share in Stride.

## 8. 5 Stride processes(5% share), 2 Stride processes(10% share), 1 Stride process(15% share), 1 Stride process(20% share), and MLFQ process
![Workload 7](/docs/project1/workload7.png)

|name|percent|
|:-:|:-:|
|MLfQ|20.158163%|
|STRIDE|79.841837%|

| name |   type|percent|   cnt|measured percent|relative percent|
|   -  |   -   |    -  |   -  |        -       |        -       |
|  MLfQ|compute|      *|201170|       20.158163|      100.000000|
|STRIDE|       |      5| 47674|        4.777155|        5.983273|
|STRIDE|       |      5| 48363|        4.846196|        6.069745|
|STRIDE|       |      5| 52215|        5.232184|        6.553186|
|STRIDE|       |      5| 53047|        5.315554|        6.657605|
|STRIDE|       |      5| 53153|        5.326176|        6.670909|
|STRIDE|       |     10| 96242|        9.643893|       12.078746|
|STRIDE|       |     10|106272|       10.648945|       13.337550|
|STRIDE|       |     15|149080|       14.938504|       18.710121|
|STRIDE|       |     20|190742|       19.113229|       23.938865|

## 9. 2 Stride processes(5% share), 2 Stride processes(10% share), 1 Stride process(20% share) and 5 MLFQ processes
![Workload 8](/docs/project1/workload8.png)

|name|percent|
|:-:|:-:|
|MLfQ|29.669%|
|STRIDE|70.331%|

|  name|   type|percent|  cnt|measured percent|relative percent|
|   -  |   -   |   -   |  -  |       -        |        -       |
|  MLfQ|compute|     * |54755|        5.714790|       19.261821|
|  MLfQ|compute|     * |56803|        5.928540|       19.982270|
|  MLfQ|compute|     * |57302|        5.980621|       20.157809|
|  MLfQ|compute|     * |57401|        5.990953|       20.192636|
|  MLfQ|compute|     * |58006|        6.054097|       20.405464|
|STRIDE|       |     5|65583|         6.844910|        9.732423|
|STRIDE|       |     5|69907|         7.296207|       10.374098|
|STRIDE|       |    10|37812|        14.383464|       20.451102|
|STRIDE|       |    10|38283|        14.432623|       20.520998|
|STRIDE|       |    20|62276|        27.373796|       38.921380|

## 10. 2 Stride processes(5% share), 1 Stride process(10% share), 1 Stride process(15% share), 1 Stride process(45% share) and 5 MLFQ processes
![Workload 9](/docs/project1/workload9.png)

|name|percent|
|:-:|:-:|
|MLfQ|21.583585%|
|STRIDE|78.416415%|

|  name|   type|percent|   cnt|measured percent|relative percent|
|  -   |    -  |   -   |   -  |       -        |       -        |
|  MLfQ|compute|      *| 38766|        3.899215|       18.065653|
|  MLfQ|compute|      *| 41367|        4.160833|       19.277765|
|  MLfQ|compute|      *| 41458|        4.169986|       19.320173|
|  MLfQ|compute|      *| 42131|        4.237679|       19.633803|
|  MLfQ|compute|      *| 50862|        5.115872|       23.702606|
|STRIDE|       |      5| 50565|        5.085999|        6.485885|
|STRIDE|       |      5| 55168|        5.548984|        7.076304|
|STRIDE|       |     10| 97617|        9.818648|       12.521164|
|STRIDE|       |     15|140924|       14.174613|       18.076078|
|STRIDE|       |     45|435342|       43.788171|       55.840568|