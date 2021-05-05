import pandas as pd
import numpy as np
data = pd.DataFrame(pd.read_csv("ms_bucket.csv"))
for row in data.values:
    print(row[1:])
    