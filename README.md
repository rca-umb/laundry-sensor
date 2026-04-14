# Laundry Sensor

A device that makes a "dumb" washer/dryer smart by alerting you when the cycle is done.

## Description

I set out to create this because I don't have a smart washer or dryer, so I have no way of knowing when my clothes are done. Obviously, I could just go check or set a timer, but I've noticed that the full cycle time varies quite a bit. This might not seem like a real problem, but because of the setup where I live, it actually is (just trust me). I hypothesized that using a really simple wireless-enabled dev board and a piezo, I could create a system which is placed on the machine, detects when the vibrations stop, and sends me a notification.

The reasons I decided to pursue this project are because it would genuinely provide value to me and there are levels to how far I could go with it, increasing the engineering complexity at each step. For example, what if I don't want it to be plugged in? What if I don't want it to use a battery (motion harvesting)? I could start with the most basic version and continuously improve.

## Project Layout

I've decided to group this repo into different iterations of the design, as I feel there is actually quite a bit that could go into designing a device like this and it might be prudent to create a design focused on just one feature, check that everything works, and then try adding something else.

## Iterations

### Testing

My initial hardware was an Adafruit Feather M0 WiFi and a cheap piezoelectric disc. The M0 is pretty sweet, its got an ATSAMD21 for the processor and an ATWINC1500 WiFI module. I actually bought this a while back to use with some of the Adafruit e-paper displays and I never really got around to using the WiFi capabilities. Thus, playing around with that and learning how set up a server were am essential focus of my initial testing.

The other thing I had to explore was what kind of values the piezo would get from my washer and dryer. To accomplish this, I set up the Feather to connect to a local WiFi network and host a simple webpage. At a set interval, the page would simple print the reading from the sensor and the time it was taken at. The time was important as it allowed me to map how the vibration levels changed to certain parts of the wash/dry cycle. This wasn't strictly required seeing as I knew how long the interval between measurements was, so I could just calculate how far from the start of a cycle a certain reading was, but for some reason, I felt like seeing an actual time would help build a mental model of how the appliance was operating. Perhaps this is because in daily life, this is the frame of reference I use when doing my laundry for real (i.e. "I started the wash at 5:15 and now it's 5:45, so it's probably like half way done").

Regardless of its necessity, getting the current time was a useful feature because it was just another thing I had to learn how to do on the Feather. I realized that up until this point, I have never actually needed the current time while working with a simple MCU. Since we were already connected to the internet, I figured there must be an API out there for getting the current time, though for some reason the idea seemed a little silly. Anyway, I found [WorldTimeAPI](www.worldtimeapi.org) which offered free access to an intuitive HTTP endpoint that could return the current time with the correct timezone based on the IP of my router, making things really easy for me. I'm actually writing this quite some time after I did this initial testing, and it seems that WorldTimeAPI has been sunset. RIP.

Here you can see the actual values from one of my live tests with a washer load:

```md
# Average Vibration Values

Value at 10:25 AM: 0.30  
Value at 10:28 AM: 1.67  
Value at 10:30 AM: 2.99  
Value at 10:32 AM: 2.29  
Value at 10:34 AM: 3.18  
Value at 10:36 AM: 3.09  
Value at 10:38 AM: 3.20  
Value at 10:40 AM: 3.42  
Value at 10:42 AM: 3.63  
Value at 10:44 AM: 3.10  
Value at 10:46 AM: 3.08  
Value at 10:48 AM: 3.38  
Value at 10:50 AM: 7.68  
Value at 10:52 AM: 1.33  
Value at 10:54 AM: 18.16  
Value at 10:56 AM: 4.03  
Value at 10:58 AM: 3.46  
Value at 11:00 AM: 3.62  
Value at 11:02 AM: 15.47  
Value at 11:04 AM: 4.61  
Value at 11:06 AM: 26.76  
Value at 11:08 AM: 24.10  
Value at 11:10 AM: 21.83  
Value at 11:12 AM: 21.99  
Value at 11:14 AM: 21.23  
Value at 11:16 AM: 9.27  
Value at 11:18 AM: 0.22  
Value at 11:20 AM: 0.19  
Value at 11:22 AM: 0.20  
Value at 11:24 AM: 0.21  
Value at 11:28 AM: 0.22  
Value at 11:33 AM: 0.01
```

And here you can see the same but with a dryer cycle:

```md
# Average Vibration Values

Value at 11:40 AM: 2.98  
Value at 11:43 AM: 113.40  
Value at 11:45 AM: 102.32  
Value at 11:47 AM: 90.83  
Value at 11:49 AM: 81.29  
Value at 11:51 AM: 74.40  
Value at 11:53 AM: 64.58  
Value at 11:55 AM: 72.63  
Value at 11:57 AM: 62.28  
Value at 11:59 AM: 57.59  
Value at 12:01 PM: 65.27  
Value at 12:03 PM: 62.82  
Value at 12:05 PM: 60.94  
Value at 12:07 PM: 63.56  
Value at 12:09 PM: 56.90  
Value at 12:11 PM: 69.36  
Value at 12:13 PM: 69.47  
Value at 12:15 PM: 91.28  
Value at 12:17 PM: 106.98  
Value at 12:19 PM: 103.14  
Value at 12:21 PM: 82.06  
Value at 12:23 PM: 0.39  
Value at 12:25 PM: 0.39  
Value at 12:27 PM: 0.12  
Value at 12:29 PM: 0.13  
Value at 12:31 PM: 0.09  
Value at 12:33 PM: 0.10  
Value at 12:35 PM: 0.12  
Value at 12:37 PM: 0.10  
Value at 12:39 PM: 0.11  
Value at 12:41 PM: 0.11  
Value at 12:43 PM: 0.14  
Value at 12:45 PM: 0.12
```

Of course, there is quite a bit of variance throughout the cycle. To my surprise, the dryer got drastically higher than the washer, but to be fair, I've never actually stuck around to watch a whole cycle of either. With an idea about what the behavior of the two machines was like and how it would register in the system, I moved out of the testing phase and into making the first prototype.

### Prototype 1

The goal of this iteration was to implement the simplest version of my main idea. With my observations from the testing stage, I felt confident that I could create use some sort of heuristic to determine when a cycle had finished based on just the sensor values. One important note is that the values I was recording during my testing phase were the average of 600 individual readings taken over the course of a minute. I could maybe use a hard cutoff at somewhere around 1, there were some things that could make this difficult:
    1. I need to prevent it getting falsely triggered during the start of the program when vibrations tend to be low.
    2. The value would have to work for the washing machine and the dryer which have different ranges.
    3. I didn't know what the variation in cycle readings was going to be over a longer period. I had only tested it out on like 2 or 3 loads of laundry. Would the values drift over time? What if I used different settings?

With these concerns in mind, I implemented a calibration period at the beginning of the code that gets the current sensor measurement (averaged as described before) and sets the notification threshold to be 10% higher than this value. The idea is that this will run before the cycle is started, to get an idea of what the ambient readings on the sensor are. The extra 10% was chosen through experimenting. It became clear that without any buffer, the system would sometimes never detect the cycle end as the measurements from the piezoelectric disc would never get as low as the were originally even when the appliance was just as still. On the other hand, if *too much* buffer was given, the system would get triggered during a slower part of the cycle (this was particularly problematic with the washing machine).

The next part was deciding how this would notify me. There is really no end to the options for doing something like this. Arduino has a platform for it, there's IFTTT, etc., but I really wanted to keep this as simple as possible. Enter SMTP; it literally has *simple* in the name! I chose to go this route not only because it is very light weight, but also because I have done some work with it in the past, so I had a bit of familiarity. Plus, getting an email sent to my inbox is a really good solution for this particular problem because it uses my phone (which I always have on me), but I wouldn't need to download a new app.

This actually ended up not being *quite* as simple as the "s" implied however. Gmail's smtp server has two ports, one for TLS and one for SSL; I was having trouble using either of them. I was using the WiFi101 Arduino library and it seemed like it didn't support the STARTTLS protocol Gmail requires. For SSL, I had to actually install SSL root certificate for Gmail onto the board using the firmware updater software that comes with the library in the Arduino IDE. This makes a lot of sense (why would a tiny dev board come with that?) but I hadn't though about it at first.

I should also mention at this point that I had been powering the Feather with a 3.7 V LiPo I had lying around. This application does not require much power at all, so you can get quite a bit of use between charges. Still, it *would* be nice if we could eliminate the need to charge it at all. It would also be really unfortunate if it died during the middle of a cycle, since then you would never get the email and it would fail to accomplish its sole task.

With all of these parts together, I happily tested it out the next few times I did my laundry. For the first three loads of laundry I did after putting this all together, it worked great! For some reason however, it started getting a little inconsistent; sometimes it would work fine, but other times the email would never send. I wasn't hosting the webserver in this iteration, so there wasn't a good way to really see what was going on. This caused me to implement an error code system using the onboard LED. Now, when I didn't get an email within a reasonable time frame, I could just go check on it (the old fashioned way), observe the blinking pattern of the LED, and then decode it to see what went wrong. This worked really well, as I was able to see the problem was I was getting 451 responses from the SMTP server when it was time to send the email. I'm not sure why this never happened the first few times I used it; I guess it was just a coincidence. Anyway, these were just server errors and Google themselves recommends to just try again when they are encountered, so I added retry logic to the code for 400 level responses.
