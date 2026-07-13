import requests
import subprocess
import os

# FFmpeg
print("Downloading FFmpeg essentials build...")

response = requests.get("https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.7z")
response.raise_for_status()

print("Extracting FFmpeg essentials build...")

with open("ffmpeg-release-essentials.7z", "wb") as f:
    f.write(response.content)

os.makedirs("files/ffmpeg", exist_ok=True)

subprocess.run(
    ["7z", "e", "ffmpeg-release-essentials.7z", "-ofiles/ffmpeg", "ffmpeg.exe", "-r", "-y"],
    check=True,
)

os.remove("ffmpeg-release-essentials.7z")

print("FFmpeg essentials build downloaded and extracted successfully.")

# VCRedist
os.makedirs("files/vcredist", exist_ok=True)

print("Downloading VCRedist (64-bit)...")
response = requests.get("https://aka.ms/vs/17/release/vc_redist.x64.exe")
response.raise_for_status()
with open("files/vcredist/vc_redist.x64.exe", "wb") as f:
    f.write(response.content)

print("Downloading VCRedist (32-bit)...")
response = requests.get("https://aka.ms/vs/17/release/vc_redist.x86.exe")
response.raise_for_status()
with open("files/vcredist/vc_redist.x86.exe", "wb") as f:
    f.write(response.content)

print("VCRedist downloaded successfully.")