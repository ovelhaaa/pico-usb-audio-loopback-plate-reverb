#ifndef REVERB_H
#define REVERB_H

#include <cstdint>
#include <vector>

class Reverb {
public:
    Reverb(float sample_rate);
    ~Reverb();

    void process(float *left, float *right, int num_samples);
    void set_enabled(bool enabled);
    void set_freeze(bool freeze_on);

private:
    struct Comb {
        std::vector<float> buffer;
        int index;
        float feedback;
        float damp;
        float last;
    };

    struct Allpass {
        std::vector<float> buffer;
        int index;
    };

    void init_filters();

    float sample_rate_;
    bool enabled_;
    bool frozen_;

    std::vector<Comb> combs_left_;
    std::vector<Comb> combs_right_;
    std::vector<Allpass> allpasses_left_;
    std::vector<Allpass> allpasses_right_;

    std::vector<float> predelay_buffer_left_;
    std::vector<float> predelay_buffer_right_;
    int predelay_index_;

    std::vector<float> original_feedback_left_;
    std::vector<float> original_feedback_right_;
};

#endif // REVERB_H
