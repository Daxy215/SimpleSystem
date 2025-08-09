import pygame
import sys
import random
import math
from pygame.locals import *

# Initialize pygame
pygame.init()
pygame.mixer.init()

# Game constants
SCREEN_WIDTH = 800
SCREEN_HEIGHT = 600
FPS = 120
BEAT_INTERVAL = 750  # milliseconds between beats
HIT_WINDOW = 150  # milliseconds to hit on beat
ATTACK_TYPES = ["HIGH", "LOW", "HEAVY"]
ATTACK_COLORS = {
    "HIGH": (255, 50, 50),    # Red
    "LOW": (50, 50, 255),     # Blue
    "HEAVY": (255, 255, 50)   # Yellow
}

# Set up the display
screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))
pygame.display.set_caption("Beat Brawler")
clock = pygame.time.Clock()

# Colors
BACKGROUND = (20, 20, 30)
UI_BACKGROUND = (40, 40, 60)
TEXT_COLOR = (220, 220, 220)
HIT_LINE_COLOR = (100, 255, 100)
PERFECT_COLOR = (255, 215, 0)

# Fonts
title_font = pygame.font.SysFont("Arial", 48, bold=True)
ui_font = pygame.font.SysFont("Arial", 24)
big_font = pygame.font.SysFont("Arial", 36, bold=True)

class Player:
    def __init__(self):
        self.health = 100
        self.max_health = 100
        self.position = (200, SCREEN_HEIGHT // 2)
        self.size = 120
        self.punching = False
        self.punch_time = 0
        self.dodging = False
        self.dodge_time = 0
        self.countering = False
        self.counter_time = 0
        self.score = 0
        self.combo = 0
        self.perfect_hits = 0
        
    def draw(self, screen):
        # Draw player character
        color = (70, 130, 180)  # Steel blue
        if self.punching:
            color = (100, 180, 255)  # Light blue when punching
        elif self.dodging:
            color = (120, 200, 120)  # Green when dodging
        elif self.countering:
            color = (255, 215, 0)  # Gold when countering
            
        pygame.draw.circle(screen, color, self.position, self.size)
        
        # Draw eyes
        pygame.draw.circle(screen, (255, 255, 255), (self.position[0] - 30, self.position[1] - 20), 15)
        pygame.draw.circle(screen, (255, 255, 255), (self.position[0] + 30, self.position[1] - 20), 15)
        pygame.draw.circle(screen, (0, 0, 0), (self.position[0] - 30, self.position[1] - 20), 7)
        pygame.draw.circle(screen, (0, 0, 0), (self.position[0] + 30, self.position[1] - 20), 7)
        
        # Draw mouth
        if self.punching or self.countering:
            pygame.draw.arc(screen, (200, 0, 0), 
                           (self.position[0] - 40, self.position[1], 80, 60), 
                           0, math.pi, 3)
        else:
            pygame.draw.arc(screen, (150, 0, 0), 
                           (self.position[0] - 40, self.position[1] + 10, 80, 60), 
                           math.pi, 2 * math.pi, 3)

    def punch(self):
        self.punching = True
        self.punch_time = pygame.time.get_ticks()
        
    def dodge(self):
        self.dodging = True
        self.dodge_time = pygame.time.get_ticks()
        
    def counter(self):
        self.countering = True
        self.counter_time = pygame.time.get_ticks()
        
    def update(self):
        current_time = pygame.time.get_ticks()
        
        # Reset punching after animation time
        if self.punching and current_time - self.punch_time > 200:
            self.punching = False
            
        # Reset dodging after animation time
        if self.dodging and current_time - self.dodge_time > 300:
            self.dodging = False
            
        # Reset countering after animation time
        if self.countering and current_time - self.counter_time > 400:
            self.countering = False

class Enemy:
    def __init__(self):
        self.health = 100
        self.max_health = 100
        self.position = (600, SCREEN_HEIGHT // 2)
        self.size = 120
        self.attacking = False
        self.attack_type = None
        self.attack_start_time = 0
        self.attack_duration = 400
        self.attack_cooldown = 1000
        self.last_attack_time = 0
        
    def draw(self, screen):
        # Draw enemy character
        color = (180, 70, 70)  # Reddish
        if self.attacking:
            if self.attack_type == "HIGH":
                color = (255, 100, 100)  # Bright red
            elif self.attack_type == "LOW":
                color = (100, 100, 255)  # Bright blue
            elif self.attack_type == "HEAVY":
                color = (255, 255, 100)  # Bright yellow
                
        pygame.draw.circle(screen, color, self.position, self.size)
        
        # Draw angry eyes
        pygame.draw.circle(screen, (255, 255, 255), (self.position[0] - 30, self.position[1] - 20), 15)
        pygame.draw.circle(screen, (255, 255, 255), (self.position[0] + 30, self.position[1] - 20), 15)
        pygame.draw.circle(screen, (0, 0, 0), (self.position[0] - 30, self.position[1] - 20), 7)
        pygame.draw.circle(screen, (0, 0, 0), (self.position[0] + 30, self.position[1] - 20), 7)
        
        # Draw angry eyebrows
        pygame.draw.line(screen, (0, 0, 0), (self.position[0] - 45, self.position[1] - 40), 
                         (self.position[0] - 15, self.position[1] - 35), 4)
        pygame.draw.line(screen, (0, 0, 0), (self.position[0] + 45, self.position[1] - 40), 
                         (self.position[0] + 15, self.position[1] - 35), 4)
        
        # Draw mouth (always angry)
        pygame.draw.arc(screen, (200, 0, 0), 
                       (self.position[0] - 40, self.position[1] + 20, 80, 60), 
                       0, math.pi, 4)
        
        # Draw attack indicator if attacking
        if self.attacking:
            indicator_size = 50
            indicator_pos = (self.position[0], self.position[1] - self.size - 30)
            pygame.draw.circle(screen, ATTACK_COLORS[self.attack_type], indicator_pos, indicator_size)
            
            # Draw attack symbol
            symbol = ""
            if self.attack_type == "HIGH":
                symbol = "↑"
            elif self.attack_type == "LOW":
                symbol = "↓"
            elif self.attack_type == "HEAVY":
                symbol = "→"
                
            symbol_surf = big_font.render(symbol, True, (0, 0, 0))
            symbol_rect = symbol_surf.get_rect(center=indicator_pos)
            screen.blit(symbol_surf, symbol_rect)
    
    def attack(self, attack_type):
        if not self.attacking and pygame.time.get_ticks() - self.last_attack_time > self.attack_cooldown:
            self.attacking = True
            self.attack_type = attack_type
            self.attack_start_time = pygame.time.get_ticks()
            self.last_attack_time = self.attack_start_time
            return True
        return False
    
    def update(self):
        current_time = pygame.time.get_ticks()
        
        # Reset attacking after duration
        if self.attacking and current_time - self.attack_start_time > self.attack_duration:
            self.attacking = False

class BeatIndicator:
    def __init__(self):
        self.beat_timer = 0
        self.next_beat = BEAT_INTERVAL
        self.beat_pulse = 0
        self.beat_ring = 0
        self.attacks = []
        self.current_attack = None
        self.current_attack_start = 0
        self.hit_markers = []
        
    def add_beat(self):
        current_time = pygame.time.get_ticks()
        if current_time - self.beat_timer > self.next_beat:
            self.beat_timer = current_time
            self.beat_pulse = 15
            self.beat_ring = 20
            
            # Add a new attack indicator (random type)
            attack_type = random.choice(ATTACK_TYPES)
            self.attacks.append({
                "type": attack_type,
                "position": SCREEN_WIDTH,
                "speed": (SCREEN_WIDTH - 400) / (BEAT_INTERVAL / 1000 * FPS)  # Move across screen in one beat interval
            })
            
            return True
        return False
    
    def update(self):
        # Update beat pulse effect
        if self.beat_pulse > 0:
            self.beat_pulse -= 0.5
            
        if self.beat_ring > 0:
            self.beat_ring -= 0.7
            
        # Update attack indicators
        for attack in self.attacks[:]:
            attack["position"] -= attack["speed"]
            
            # Remove if off screen
            if attack["position"] < 0:
                self.attacks.remove(attack)
                
        # Update hit markers
        for marker in self.hit_markers[:]:
            marker["time"] -= 1
            if marker["time"] <= 0:
                self.hit_markers.remove(marker)
    
    def draw(self, screen):
        # Draw beat bar
        pygame.draw.rect(screen, UI_BACKGROUND, (0, SCREEN_HEIGHT - 100, SCREEN_WIDTH, 100))
        
        # Draw hit line
        pygame.draw.line(screen, HIT_LINE_COLOR, (400, SCREEN_HEIGHT - 100), (400, SCREEN_HEIGHT), 3)
        
        # Draw beat indicators
        for attack in self.attacks:
            pygame.draw.circle(screen, ATTACK_COLORS[attack["type"]], 
                             (int(attack["position"]), SCREEN_HEIGHT - 50), 
                             25)
            
            # Draw attack symbol
            symbol = ""
            if attack["type"] == "HIGH":
                symbol = "↑"
            elif attack["type"] == "LOW":
                symbol = "↓"
            elif attack["type"] == "HEAVY":
                symbol = "→"
                
            symbol_surf = ui_font.render(symbol, True, (0, 0, 0))
            symbol_rect = symbol_surf.get_rect(center=(int(attack["position"]), SCREEN_HEIGHT - 50))
            screen.blit(symbol_surf, symbol_rect)
            
            # Draw glow if near hit line
            if abs(attack["position"] - 400) < 50:
                glow_size = 25 + int(5 * math.sin(pygame.time.get_ticks() / 100))
                pygame.draw.circle(screen, (200, 255, 200), 
                                 (int(attack["position"]), SCREEN_HEIGHT - 50), 
                                 glow_size, 2)
        
        # Draw hit markers
        for marker in self.hit_markers:
            color = PERFECT_COLOR if marker["perfect"] else (200, 200, 200)
            text = "PERFECT!" if marker["perfect"] else "GOOD"
            marker_surf = ui_font.render(text, True, color)
            screen.blit(marker_surf, (410, SCREEN_HEIGHT - 80))
            
        # Draw beat pulse effect
        if self.beat_pulse > 0:
            pulse_size = 10 + int(self.beat_pulse)
            pygame.draw.circle(screen, (100, 255, 100), (400, SCREEN_HEIGHT - 50), pulse_size)
            
        if self.beat_ring > 0:
            pygame.draw.circle(screen, (100, 255, 100), (400, SCREEN_HEIGHT - 50), 
                             int(self.beat_ring), 2)
    
    def check_hit(self, attack_type):
        current_time = pygame.time.get_ticks()
        
        for attack in self.attacks[:]:
            if attack["type"] == attack_type and abs(attack["position"] - 400) < 50:
                distance = abs(attack["position"] - 400)
                perfect = distance < 15
                
                # Add hit marker
                self.hit_markers.append({
                    "time": 30,  # frames to display
                    "perfect": perfect
                })
                
                # Remove the attack
                self.attacks.remove(attack)
                
                return True, perfect
        return False, False

def draw_health_bar(screen, x, y, width, height, health, max_health):
    # Draw background
    pygame.draw.rect(screen, (80, 80, 80), (x, y, width, height))
    
    # Draw health
    health_width = int((health / max_health) * width)
    health_color = (50, 200, 50)  # Green
    if health < max_health * 0.3:
        health_color = (220, 50, 50)  # Red
    
    pygame.draw.rect(screen, health_color, (x, y, health_width, height))
    
    # Draw border
    pygame.draw.rect(screen, (200, 200, 200), (x, y, width, height), 2)
    
    # Draw health text
    health_text = ui_font.render(f"{health}/{max_health}", True, TEXT_COLOR)
    screen.blit(health_text, (x + width // 2 - health_text.get_width() // 2, 
                             y + height // 2 - health_text.get_height() // 2))

def draw_ui(screen, player, enemy, beat_indicator):
    # Draw title
    title_surf = title_font.render("BEAT BRAWLER", True, (255, 215, 0))
    screen.blit(title_surf, (SCREEN_WIDTH // 2 - title_surf.get_width() // 2, 20))
    
    # Draw player health
    pygame.draw.rect(screen, UI_BACKGROUND, (20, 20, 200, 30))
    draw_health_bar(screen, 20, 20, 200, 30, player.health, player.max_health)
    
    # Draw enemy health
    pygame.draw.rect(screen, UI_BACKGROUND, (SCREEN_WIDTH - 220, 20, 200, 30))
    draw_health_bar(screen, SCREEN_WIDTH - 220, 20, 200, 30, enemy.health, enemy.max_health)
    
    # Draw score and combo
    score_text = ui_font.render(f"SCORE: {player.score}", True, TEXT_COLOR)
    screen.blit(score_text, (SCREEN_WIDTH // 2 - score_text.get_width() // 2, 25))
    
    if player.combo > 1:
        combo_color = (255, 215, 0) if player.combo >= 5 else (255, 150, 50)
        combo_text = big_font.render(f"{player.combo} COMBO!", True, combo_color)
        screen.blit(combo_text, (SCREEN_WIDTH // 2 - combo_text.get_width() // 2, 60))
    
    # Draw controls help
    controls_text = ui_font.render("CONTROLS: ↑ PUNCH   ↓ DODGE   → COUNTER", True, (180, 180, 220))
    screen.blit(controls_text, (SCREEN_WIDTH // 2 - controls_text.get_width() // 2, SCREEN_HEIGHT - 30))
    
    # Draw beat indicator
    beat_indicator.draw(screen)

def main():
    player = Player()
    enemy = Enemy()
    beat_indicator = BeatIndicator()
    
    # Game state
    running = True
    game_over = False
    last_attack_time = pygame.time.get_ticks()
    
    # Main game loop
    while running:
        current_time = pygame.time.get_ticks()
        
        # Handle events
        for event in pygame.event.get():
            if event.type == QUIT:
                running = False
                
            if event.type == KEYDOWN:
                if not game_over:
                    if event.key == K_UP:
                        player.punch()
                        hit, perfect = beat_indicator.check_hit("HIGH")
                        if hit:
                            enemy.health -= 15 if perfect else 10
                            player.score += 100 if perfect else 50
                            player.combo += 1
                            player.perfect_hits += 1 if perfect else 0
                        else:
                            player.combo = 0
                            
                    elif event.key == K_DOWN:
                        player.dodge()
                        hit, perfect = beat_indicator.check_hit("LOW")
                        if hit:
                            enemy.health -= 15 if perfect else 10
                            player.score += 100 if perfect else 50
                            player.combo += 1
                            player.perfect_hits += 1 if perfect else 0
                        else:
                            player.combo = 0
                            
                    elif event.key == K_RIGHT:
                        player.counter()
                        hit, perfect = beat_indicator.check_hit("HEAVY")
                        if hit:
                            enemy.health -= 20 if perfect else 15
                            player.score += 150 if perfect else 75
                            player.combo += 1
                            player.perfect_hits += 1 if perfect else 0
                        else:
                            player.combo = 0
                else:
                    # Restart game on any key press
                    if event.key:
                        player = Player()
                        enemy = Enemy()
                        beat_indicator = BeatIndicator()
                        game_over = False
        
        # Add beats
        beat_indicator.add_beat()
        
        # Enemy attack logic
        if not game_over and current_time - last_attack_time > 2000:
            attack_type = random.choice(ATTACK_TYPES)
            if enemy.attack(attack_type):
                last_attack_time = current_time
                
                # Player takes damage if not defending
                if attack_type == "HIGH" and not player.punching:
                    player.health -= 10
                    player.combo = 0
                elif attack_type == "LOW" and not player.dodging:
                    player.health -= 10
                    player.combo = 0
                elif attack_type == "HEAVY" and not player.countering:
                    player.health -= 15
                    player.combo = 0
        
        # Update game objects
        player.update()
        enemy.update()
        beat_indicator.update()
        
        # Check game over
        if player.health <= 0 or enemy.health <= 0:
            game_over = True
        
        # Draw everything
        screen.fill(BACKGROUND)
        
        # Draw arena
        pygame.draw.line(screen, (60, 60, 80), (0, SCREEN_HEIGHT // 2), (SCREEN_WIDTH, SCREEN_HEIGHT // 2), 2)
        
        # Draw characters
        player.draw(screen)
        enemy.draw(screen)
        
        # Draw UI
        draw_ui(screen, player, enemy, beat_indicator)
        
        # Draw game over screen
        if game_over:
            overlay = pygame.Surface((SCREEN_WIDTH, SCREEN_HEIGHT), pygame.SRCALPHA)
            overlay.fill((0, 0, 0, 180))
            screen.blit(overlay, (0, 0))
            
            result_text = "VICTORY!" if enemy.health <= 0 else "DEFEAT!"
            result_color = (50, 200, 50) if enemy.health <= 0 else (200, 50, 50)
            result_surf = title_font.render(result_text, True, result_color)
            screen.blit(result_surf, (SCREEN_WIDTH // 2 - result_surf.get_width() // 2, 150))
            
            score_surf = big_font.render(f"Final Score: {player.score}", True, TEXT_COLOR)
            screen.blit(score_surf, (SCREEN_WIDTH // 2 - score_surf.get_width() // 2, 250))
            
            perfect_surf = big_font.render(f"Perfect Hits: {player.perfect_hits}", True, PERFECT_COLOR)
            screen.blit(perfect_surf, (SCREEN_WIDTH // 2 - perfect_surf.get_width() // 2, 300))
            
            restart_surf = ui_font.render("Press any key to restart", True, (180, 180, 220))
            screen.blit(restart_surf, (SCREEN_WIDTH // 2 - restart_surf.get_width() // 2, 400))
        
        pygame.display.flip()
        clock.tick(FPS)
    
    pygame.quit()
    sys.exit()

if __name__ == "__main__":
    main()