import os
from PIL import ImageFont
from PIL import Image
from PIL import ImageDraw
import urllib
import random

sock = urllib.urlopen("http://192.168.101.2/app/lichtkrant/stroom.php")
htmlSource = sock.read()
sock.close()
sock = urllib.urlopen("http://192.168.101.2/app/lichtkrant/co2.php")
httpco2 = sock.read()
sock.close()
sock = urllib.urlopen("http://192.168.101.2/app/lichtkrant/temp.php")
httptemp = sock.read()
sock.close()
sock = urllib.urlopen("http://192.168.101.2/app/lichtkrant/bar.php")
httpbar = sock.read()
sock.close()
sock = urllib.urlopen("http://192.168.101.2/app/lichtkrant/hum.php")
httphum = sock.read()
sock.close()

font = ImageFont.truetype("/usr/share/fonts/truetype/freefont/FreeSans.ttf", 12)

im = Image.new("RGB", (96, 64), "black")
draw = ImageDraw.Draw(im)


x = 0;


stroom = "Stroom " +htmlSource
co2 = "Co2 " +httpco2
temp = "Tempratuur " +httptemp
bar = "Bar " +httpbar
hum = "Vochtigheid " +httphum

coloring = (58, 58, 251)

draw.text((0, 0),stroom,coloring,font=font)
draw.text((x, 12),co2,coloring, font=font)
draw.text((0, 24),temp,coloring, font=font)
draw.text((0, 36),bar,coloring, font=font)
draw.text((0, 48),hum,coloring, font=font)

im.save("/home/pi/images/openhab.ppm")

os.system("/home/pi/led-matrix/led-matrix -t 20 -m 0 -V -D 1 /home/pi/images/openhab.ppm -p4")
