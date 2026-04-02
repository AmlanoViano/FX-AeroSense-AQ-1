import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

file_path = r"C:\Users\ROG\Desktop\Data\data.csv"

df = pd.read_csv(file_path)

print(df)



plt.figure(figsize=(10,5))
plt.plot(df["Timestamp"], df["NOX_ADC_RED"], color="red")
plt.xlabel("Time (s)")
plt.ylabel("NOX_ADC_RED")
plt.title("NOX_ADC_RED Over Time")
plt.show()