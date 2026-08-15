// Querying DOM elements
let now_playing = document.querySelector(".now-playing");
let track_art = document.querySelector(".track-art");
let track_name = document.querySelector(".track-name");
let track_artist = document.querySelector(".track-artist");

let playpause_btn = document.querySelector(".playpause-track");
let next_btn = document.querySelector(".next-track");
let prev_btn = document.querySelector(".prev-track");
let repeat_btn = document.querySelector(".repeat-track"); // Reference for the repeat button

let seek_slider = document.querySelector(".seek-slider");
let volume_slider = document.querySelector(".volume-slider");
let curr_time = document.querySelector(".current-time");
let total_duration = document.querySelector(".total-duration");
let randomIcon = document.querySelector(".fa-random");

let curr_track = document.createElement("audio");

let track_index = 0;
let isPlaying = false;
let isRandom = false;
let isRepeat = false; // New variable for repeat mode
let updateTimer;

// Music list
const music_list = [
    {
        img: "DemonSlayer_assets/homura.jpg",
        name: "Homura",
        artist: "Lisa",
        music: "DemonSlayer_assets/Homura.mp3",
    },
    {
        img: "DemonSlayer_assets/H.jpg",
        name: "Akeboshi",
        artist: "Lisa",
        music: "DemonSlayer_assets/Akeboshi.mp3",
    }
];

// Load the first track
loadTrack(track_index);

// Function to load a track
function loadTrack(track_index) {
    clearInterval(updateTimer);
    
    // Update track source and metadata
    curr_track.src = music_list[track_index].music;
    curr_track.load();

    track_art.style.backgroundImage = "url(" + music_list[track_index].img + ")";
    track_name.textContent = music_list[track_index].name;
    track_artist.textContent = music_list[track_index].artist;

    now_playing.textContent = 
    "Playing Music " + (track_index + 1) + " of " + music_list.length;

    // Set the update timer for the seek bar
    updateTimer = setInterval(setUpdate, 1000);

    // Event listener to play the next track or repeat the current track
    curr_track.addEventListener("ended", nextTrackOrRepeat);
}

// Play or pause the track
function playpauseTrack() {
    if (!isPlaying) {
        playTrack();
    } else {
        pauseTrack();
    }
}

// Play the current track
function playTrack() {
    curr_track.play();
    isPlaying = true;
    playpause_btn.innerHTML = '<i class="fa fa-pause-circle fa-5x"></i>';
}

// Pause the current track
function pauseTrack() {
    curr_track.pause();
    isPlaying = false;
    playpause_btn.innerHTML = '<i class="fa fa-play-circle fa-5x"></i>';
}

// Move to the next track or repeat the current one if repeat mode is enabled
function nextTrackOrRepeat() {
    if (isRepeat) {
        // Replay the current track if repeat mode is on
        curr_track.currentTime = 0; // Reset to the start
        playTrack(); // Play the track again
    } else {
        nextTrack();
    }
}

// Move to the next track
function nextTrack() {
    if (track_index < music_list.length - 1) {
        track_index += 1;
    } else {
        track_index = 0;
    }
    loadTrack(track_index);
    playTrack();
}

// Move to the previous track
function prevTrack() {
    if (track_index > 0) {
        track_index -= 1;
    } else {
        track_index = music_list.length - 1;
    }
    loadTrack(track_index);
    playTrack();
}

// Update the seek slider
function setUpdate() {
    let seekPosition = 0;
    
    // Only update if the track is playing
    if (!isNaN(curr_track.duration)) {
        seekPosition = curr_track.currentTime * (100 / curr_track.duration);
        seek_slider.value = seekPosition;

        let currentMinutes = Math.floor(curr_track.currentTime / 60);
        let currentSeconds = Math.floor(curr_track.currentTime - currentMinutes * 60);
        let durationMinutes = Math.floor(curr_track.duration / 60);
        let durationSeconds = Math.floor(curr_track.duration - durationMinutes * 60);

        if (currentSeconds < 10) { currentSeconds = "0" + currentSeconds; }
        if (durationSeconds < 10) { durationSeconds = "0" + durationSeconds; }
        if (currentMinutes < 10) { currentMinutes = "0" + currentMinutes; }
        if (durationMinutes < 10) { durationMinutes = "0" + durationMinutes; }

        curr_time.textContent = currentMinutes + ":" + currentSeconds;
        total_duration.textContent = durationMinutes + ":" + durationSeconds;
    }
}

// Seek to the desired position
function seekTo() {
    let seekto = curr_track.duration * (seek_slider.value / 100);
    curr_track.currentTime = seekto;
}

// Set the volume
function setVolume() {
    curr_track.volume = volume_slider.value / 100;
}

// Toggle random mode
function randomTrack() {
    isRandom = !isRandom;
    randomIcon.classList.toggle('randomActive');
}

// New Functionality for Repeat Button
function repeatTrack() {
    let current_index = track_index;
    loadTrack(current_index);
    playTrack();
}

// Bind repeatTrack function to repeat button click event
repeat_btn.addEventListener("click", repeatTrack);

const readmore = document.querySelector('.read-more-btn');
const lyrics = document.querySelector('.lyrics');
const padding = document.querySelector('.padding');

if (readmore && lyrics) {
    readmore.addEventListener('click', () => {
        lyrics.classList.toggle('show-more');
        padding.classList.toggle('show-more');
    });
} else {
    console.error('Elements not found: .read-more-btn or .lyrics');
}
