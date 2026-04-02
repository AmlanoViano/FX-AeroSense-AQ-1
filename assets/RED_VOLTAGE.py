import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

file_path = r"C:\Users\ROG\Desktop\Data\data.csv"

df = pd.read_csv(file_path)

print(df)

#Line chart for showing trends of PM1.0, PM2.5
plt.figure(figsize=(10,5))
plt.plot(df["Timestamp"], df["RED_VOLTAGE"], color="orange")
plt.xlabel("Time (s)")
plt.ylabel("RED_VOLTAGE (Volts)")
plt.title("RED_VOLTAGE Over Time")
plt.show()