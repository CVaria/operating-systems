
# !/ usr / bin / env python from Tkinter 

import * 

#For plot . . . 
import os 
import numpy as np 
import datetime from matplotlib 
import pyplot as plt from matplotlib
import animation 

# . . . for plot

top = Tk() 
#Fix title + window size 

top . wm_title (”Sensors ␣ Plots ”) 
width=400 
height=180 
top . geometry( ’ {}x{} ’ . format(width , height ) ) 

global fName, sSelect , mSelect 
sSelect = 0 
mSelect = 0 
fName=”” 

#########−−PLOT BEGIN−−############# 
def init () : 
    global line 
    line . set_data ([] , [])
    return line , 

def animate( i ) : 
    global fName 
    global xdata , ydata , ax 
    fp=open(fName) 
    tmpY = fp . readline () 
    fp . close () 
    y=float (tmpY) 
    # Collect data into x and y lists
    xdata . append( i )
    ydata . append(y)
    xmin , xmax = ax. get_xlim () 
    ymin , ymax = ax. get_ylim ()
    
    #changing the xmax dynamically 
    if i >= xmax: 
        ax. set_xlim (xmin , xmax+(xmax/2) ) 
        ax. figure . canvas .draw()
        
    #changing the ymax dynamically 
    if y >= ymax: 
        ax. set_ylim (ymin , y+(y/10) ) 
        ax. figure . canvas .draw()

    line . set_data (xdata , ydata) 
    return line ,

#When closed plot , allow user to choose new type of plot 
def handle_close ( evt ) : 
    global c0 , c1 , m0, m1, m2, fName, mSelect , sSelect 
    c0 . deselect () 
    c1 . deselect () 
    m0. deselect () 
    m1. deselect () 
    m2. deselect () 
    c0 . config ( state=’normal ’) 
    c1 . config ( state=’normal ’) 
    m0. config ( state=’normal ’) 
    m1. config ( state=’normal ’) 
    m2. config ( state=’normal ’) 
    fName=”” 
    mSelect = 0 
    sSelect = 0

#Disable buttons when user has chosen sensor and measurement 
def disable_all () : 
    global c0 , c1 , m0, m1, m2 
    c0 . config ( state=’ disable ’) 
    c1 . config ( state=’ disable ’) 
    m0. config ( state=’ disable ’) 
    m1. config ( state=’ disable ’) 
    m2. config ( state=’ disable ’)

def do_plot () : 
    # initial max x axis 
    init_xlim = 5 
    init_ylim = 3
    global xdata , ydata , ax , fName 
    fig = plt . figure (fName) 
    fig . canvas . mpl_connect( ’close_event ’ , handle_close ) 
    ax = plt . axes(xlim=(0 , init_xlim ) , ylim=(0 , init_ylim ) )

    #Create appropriate Labels 
    if ”temp” in fName: 
        yLabel=”Temperature” 
    elif ”batt” in fName: 
        yLabel=”Voltage” 
    else : 
        yLabel=”Light” 
    plt . ylabel ( yLabel ) 
    plt . xlabel (”Time␣ ( sec )”) 
    #And title of plot 
    if ”0” in fName: 
        sensorNo=”0” 
    else : 
        sensorNo=”1” 
    
    plt . title ( yLabel+” from sensor No”+sensorNo) 
    ax. grid ()

    #Disable buttons until plot is closed 
    disable_all ()

    global line 
    line , = ax. plot ([] , [] , lw=1) 
    xdata , ydata = [] , []

    anim = animation . FuncAnimation( fig , animate , init_func=init , 
        frames=200, interval =1000, blit=False ) 
    plt .show() 
    
##########−−PLOT END−−############## 

#Sensor CheckButtons functions 
def sensor0Active () : 
    if ( sensor0 . get () ) : 
        global fName, sSelect 
        if sSelect == 0: # first sensor selection 
            sSelect =1 
            if fName==”” :
                fName=”/dev/ lunix0” 
            else : 
                fName=”/dev/ lunix0”+fName 
                do_plot () 
            else : #update existing sensor selection 
                fName=”/dev/ lunix0”
                
def sensor1Active () : 
    if ( sensor1 . get () ) : 
        global fName, sSelect 
        if sSelect == 0: # first selection 
            sSelect = 1 
            if fName==”” : 
                fName=”/dev/ lunix1” 
            else : 
                fName=”/dev/ lunix1”+fName 
                do_plot () 
        else : #update existing one 
            fName=”/dev/ lunix1”

#Measurement type CheckButtons functions 
def MeasTemp() : 
    if (Temp. get () ) : 
        global fName, mSelect 
        if mSelect == 0: 
            mSelect=1 
            if fName==”” : 
                fName=”−temp”
            else : 
                fName=fName+”−temp” 
                do_plot () 
        else : #update existing one 
            fName=”−temp”

def MeasBatt () : 
    if ( Batt . get () ) : 
        global fName, mSelect 
        if mSelect == 0: 
            mSelect=1 
            if fName==”” : 
                fName=”−batt ”
            else : 
                fName=fName+”−batt” 
                do_plot () 
        else : #update existing one 
            fName=”−batt”
            
def MeasLight () : 
    if ( Light . get () ) : 
        global fName, mSelect 
        if mSelect == 0: 
            mSelect=1 
            if fName==”” : 
                fName=”−light ”
            else : 
                fName=fName+”−light ” 
                do_plot ()
    else : #update existing one 
        fName=”−light ”
        
#CheckButtons for Sensors 
sensor0 = IntVar () 
sensor1 = IntVar ()

c0 = Checkbutton(top , text=”Sensor ␣0” , variable=sensor0 , command= sensor0Active , font=( ’Symbol ’ , 12) ) 
c0 . grid (row=11, column=0, columnspan=2, rowspan=1) 
c1 = Checkbutton(top , text=”Sensor ␣1” , variable=sensor1 , command= sensor1Active , font=( ’Symbol ’ , 12) ) 
c1 . grid (row=12, column=0, columnspan=2, rowspan=1)

#Checkbuttons for measurement type 
Temp = IntVar () 
Batt = IntVar () 
Light = IntVar ()

w = Message(top , text=”Sensor ␣Number” , width = width/2 , font=( ’Symbol ’ , 14, ’ italic ’) ) . grid (row=10, column=0, rowspan=1, columnspan=2) 
w = Message(top , text=”Measurement␣Type” , width = width/2 , font=( ’ Symbol ’ , 14, ’ italic ’) ) . grid (row=10, column=3, rowspan=1, columnspan=2)
    
m0 = Checkbutton(top , text=”Temperature” , variable=Temp, command= MeasTemp, font=( ’Symbol ’ , 12) ) 
m0. grid (row=11, column=3, columnspan=2, rowspan=1, sticky=W) 
m1 = Checkbutton(top , text=”Battery” , variable=Batt , command=MeasBatt , font=( ’Symbol ’ , 12) ) 
m1. grid (row=12, column=3, columnspan=2, rowspan=1, sticky=W) 
m2 = Checkbutton(top , text=”Light” , variable=Light , command=MeasLight , font=( ’Symbol ’ , 12) ) 
m2. grid (row=13, column=3,columnspan=2, rowspan=1, sticky=W)

top . mainloop ()

