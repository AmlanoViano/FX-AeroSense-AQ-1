import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

file_path = r"C:\Users\ROG\Desktop\Data\data.csv"

df = pd.read_csv(file_path)

print(df)

plt.figure(figsize=(8,5))
plt.scatter(df["PM2.5"], df["NOX_ADC_RED"], color="purple")
plt.xlabel("PM2.5")
plt.ylabel("NOX_ADC_RED")
plt.title("PM2.5 vs NOX_ADC_RED")
plt.show()